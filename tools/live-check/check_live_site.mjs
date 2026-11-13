#!/usr/bin/env node
/*
 * check_live_site.mjs — Load the published game in headless Chromium and
 * fail when it does not boot cleanly.
 *
 * Build & Release runs this after Pages deploy ("Live site check"). It
 * also runs locally:
 *
 *   cd tools/live-check && npm ci && npx playwright install chromium
 *   node check_live_site.mjs --expect-version "$(git rev-parse HEAD)"
 *
 * Steps, each a failure when it does not happen:
 *   1. The page's asset links (super-mango.js?v=..., .wasm, .data) carry the
 *      expected version. GitHub Pages and its CDN take a moment to serve a
 *      new deployment, so this is retried until --timeout-seconds.
 *   2. The canvas reaches its "running" label and the loading status line
 *      is hidden.
 *   3. A held ArrowRight leaves the game running and the canvas changes.
 * Throughout, any console error, uncaught page error or failed request
 * fails the check, except the messages in KNOWN_NOISE.
 *
 * Exit status: 0 pass, 1 check failed, 2 bad arguments.
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';

const DEFAULT_URL = 'https://super-mango-editor.jonathanperis.tech/super-mango.html';
const RUNNING_LABEL = 'Super Mango game canvas - running';

/* Messages Chromium prints for every healthy load of this page:
 * - audio waits for a user gesture (web/audio-unlock.js resumes it);
 * - raylib's miniaudio uses ScriptProcessorNode, because the AudioWorklet
 *   path needs cross-origin isolation that GitHub Pages cannot provide
 *   (docs/wiki/build-system.md, "Web audio");
 * - SwiftShader reports the GPU stall of a ReadPixels call.
 * Each is matched from the start of the message, so a different error that
 * merely mentions one of these words still fails. */
const KNOWN_NOISE = [
    'The AudioContext was not allowed to start. It must be resumed (or created) after a user gesture on the page.',
    'The ScriptProcessorNode is deprecated. Use AudioWorkletNode instead.',
];
const KNOWN_NOISE_PATTERNS = [
    /^\[\.WebGL-0x[0-9a-f]+\]GL Driver Message \(OpenGL, Performance, GL_CLOSE_PATH_NV, High\): GPU stall due to ReadPixels( \(this message will no longer repeat\))?$/,
];

function isKnownNoise(text) {
    return KNOWN_NOISE.some(message => text.startsWith(message)) ||
        KNOWN_NOISE_PATTERNS.some(pattern => pattern.test(text));
}

function usage(message) {
    if (message) console.error(`check_live_site: ${message}`);
    console.error('usage: check_live_site.mjs [--url URL] [--expect-version SHA] ' +
                  '[--timeout-seconds N] [--out DIR]');
    process.exit(2);
}

function parseArgs(argv) {
    const options = { url: DEFAULT_URL, expectVersion: '', timeoutSeconds: 300, out: 'out/live-check' };
    for (let i = 0; i < argv.length; i++) {
        const flag = argv[i];
        const value = argv[i + 1];
        if (flag === '--help' || flag === '-h') usage();
        if (value === undefined || value.startsWith('--')) usage(`${flag} needs a value`);
        if (flag === '--url') options.url = value;
        else if (flag === '--expect-version') options.expectVersion = value.toLowerCase();
        else if (flag === '--timeout-seconds') options.timeoutSeconds = Number(value);
        else if (flag === '--out') options.out = value;
        else usage(`unknown option ${flag}`);
        i++;
    }
    if (!Number.isFinite(options.timeoutSeconds) || options.timeoutSeconds <= 0)
        usage('--timeout-seconds must be a positive number');
    if (options.expectVersion && !/^[0-9a-f]{7,40}$/.test(options.expectVersion))
        usage('--expect-version must be a commit hash (7 to 40 hex digits)');
    return options;
}

/* The versions in the page's asset links. One per distinct value. */
function assetVersions(html) {
    const found = new Set();
    for (const match of html.matchAll(/super-mango\.(?:js|wasm|data)\?v=([0-9a-zA-Z._-]+)/g))
        found.add(match[1].toLowerCase());
    return [...found];
}

/* A short hash on either side matches the full one it starts. */
function sameVersion(served, expected) {
    return served.length >= 7 && (served.startsWith(expected) || expected.startsWith(served));
}

/* A query string of its own keeps the CDN from answering with an older copy. */
function freshUrl(url) {
    const fresh = new URL(url);
    fresh.searchParams.set('live-check', String(Date.now()));
    return fresh.toString();
}

const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

async function waitForVersion(options, deadline) {
    let last;  // why the latest attempt did not match; set by every attempt
    for (;;) {
        try {
            const response = await fetch(freshUrl(options.url), { cache: 'no-store' });
            if (!response.ok) {
                last = `HTTP ${response.status}`;
            } else {
                const versions = assetVersions(await response.text());
                if (!versions.length) last = 'no versioned asset links in the page';
                else if (!options.expectVersion) return versions;
                else if (versions.every(version => sameVersion(version, options.expectVersion))) return versions;
                else last = `serves ${versions.join(', ')}`;
            }
        } catch (error) {
            last = error.message;
        }
        if (Date.now() >= deadline)
            throw new Error(`the page never served version ${options.expectVersion || '(any)'}: ${last}`);
        await sleep(10000);
    }
}

async function canvasState(page) {
    return page.evaluate(() => {
        const canvas = document.querySelector('canvas');
        const status = document.getElementById('status');
        return {
            label: canvas ? canvas.getAttribute('aria-label') || '' : '',
            statusHidden: !status || getComputedStyle(status).display === 'none',
        };
    });
}

async function main() {
    const options = parseArgs(process.argv.slice(2));
    const deadline = Date.now() + options.timeoutSeconds * 1000;
    const problems = [];
    const noise = [];
    mkdirSync(options.out, { recursive: true });

    const versions = await waitForVersion(options, deadline);
    console.log(`check_live_site: ${options.url} serves version ${versions.join(', ')}`);

    const browser = await chromium.launch({
        args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'],
    });
    try {
        const page = await browser.newPage({ viewport: { width: 1000, height: 700 } });
        page.on('console', message => {
            const type = message.type();
            if (type !== 'error' && type !== 'warning') return;
            const text = message.text();
            if (isKnownNoise(text)) noise.push(text);
            else if (type === 'error') problems.push(`console error: ${text}`);
        });
        page.on('pageerror', error => problems.push(`page error: ${error.message}`));
        page.on('requestfailed', request =>
            problems.push(`failed request: ${request.url()} (${request.failure()?.errorText || 'unknown'})`));
        page.on('response', response => {
            if (response.status() >= 400) problems.push(`HTTP ${response.status()}: ${response.url()}`);
        });

        await page.goto(freshUrl(options.url), { waitUntil: 'load', timeout: 60000 });

        let state = await canvasState(page);
        const bootDeadline = Math.min(deadline, Date.now() + 60000);
        while (!(state.label.startsWith(RUNNING_LABEL) && state.statusHidden) && Date.now() < bootDeadline) {
            await sleep(500);
            state = await canvasState(page);
        }
        await page.screenshot({ path: path.join(options.out, 'boot.png') });
        if (!state.label.startsWith(RUNNING_LABEL))
            problems.push(`the canvas never reached the running label (last: "${state.label}")`);
        if (!state.statusHidden) problems.push('the loading status line is still shown');

        if (!problems.length) {
            const canvas = page.locator('canvas');
            await canvas.click({ position: { x: 400, y: 300 } });
            const before = await canvas.screenshot();
            await page.keyboard.down('ArrowRight');
            await sleep(1500);
            await page.keyboard.up('ArrowRight');
            await sleep(250);
            const after = await canvas.screenshot();
            await page.screenshot({ path: path.join(options.out, 'moved.png') });
            state = await canvasState(page);
            if (!state.label.startsWith(RUNNING_LABEL))
                problems.push(`the game stopped after input (label: "${state.label}")`);
            if (before.equals(after)) problems.push('the canvas did not change while ArrowRight was held');
        }
    } finally {
        await browser.close();
    }

    const report = { url: options.url, versions, problems, knownNoise: noise };
    writeFileSync(path.join(options.out, 'report.json'), JSON.stringify(report, null, 2) + '\n');
    console.log(`check_live_site: ${noise.length} known noise message(s) ignored`);
    if (problems.length) {
        for (const problem of problems) console.error(`check_live_site: ${problem}`);
        console.error(`check_live_site: FAILED (${problems.length} problem(s))`);
        process.exit(1);
    }
    console.log('check_live_site: ok (booted, running, no console errors)');
}

main().catch(error => {
    console.error(`check_live_site: FAILED: ${error.message}`);
    process.exit(1);
});
