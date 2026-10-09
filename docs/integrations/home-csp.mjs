// Content-Security-Policy for the home page, the page that hosts the game.
//
// Astro's built-in `security.csp` applies to every page and pins style-src to
// hashes, which would block the game's injected touch-control <style> and the
// docs pages' optional analytics. This integration covers only index.html: once
// the build has written the final HTML, it hashes each executable inline script
// (Astro's inlined module script and the game bootstrap) and inserts a matching
// <meta> policy before them. tools/check_docs_site.py re-verifies the result.
import { createHash } from "node:crypto";
import { readFile, writeFile } from "node:fs/promises";

const INLINE_PLACEHOLDER = "'sha256-MANGO_HOME_INLINE_SCRIPTS'";

// 'self' serves the Astro bundle and the game's super-mango.js/.wasm/.data;
// 'wasm-unsafe-eval' compiles WebAssembly without allowing JavaScript eval.
// Styles keep 'unsafe-inline' because the game injects its touch-control CSS.
// Fonts are self-hosted (see BaseLayout.astro), so no font or style host is
// allowed beyond 'self'.
export const HOME_CSP = [
    "default-src 'self'",
    `script-src 'self' 'wasm-unsafe-eval' ${INLINE_PLACEHOLDER}`,
    "style-src 'self' 'unsafe-inline'",
    "font-src 'self'",
    "img-src 'self' data: blob:",
    "media-src 'self' blob:",
    "connect-src 'self'",
    "object-src 'none'",
    "base-uri 'none'",
    "form-action 'none'",
].join("; ");

// HTML ends a script at "</script" plus anything up to ">" (even
// "</script\t\n foo>"), so the end tag accepts [^>]* rather than just spaces.
const SCRIPT = /<script\b([^>]*)>([\s\S]*?)<\/script\b[^>]*>/gi;
const TYPE = /\btype\s*=\s*["']?([^"'\s>]+)/i;
const JS_TYPES = new Set(["module", "text/javascript", "application/javascript"]);

/** CSP hash sources for every inline script the browser would execute. */
export function inlineScriptHashes(html) {
    const hashes = [];
    for (const [, attrs, body] of html.matchAll(SCRIPT)) {
        if (/\bsrc\s*=/i.test(attrs)) continue;          // covered by 'self'
        const type = TYPE.exec(attrs)?.[1]?.toLowerCase();
        if (type && !JS_TYPES.has(type)) continue;        // e.g. JSON-LD data
        const digest = createHash("sha256").update(body, "utf8").digest("base64");
        hashes.push(`'sha256-${digest}'`);
    }
    return hashes;
}

export function homeCsp() {
    return {
        name: "super-mango-home-csp",
        hooks: {
            "astro:build:done": async ({ dir, logger }) => {
                const file = new URL("index.html", dir);
                const html = await readFile(file, "utf8");
                const hashes = inlineScriptHashes(html);
                const charset = /<meta charset="utf-8"\s*\/?>/i;
                if (!hashes.length || !charset.test(html) || /http-equiv=["']?content-security-policy/i.test(html)) {
                    throw new Error("home-csp: expected inline scripts, a charset meta and no existing CSP in index.html");
                }
                const policy = HOME_CSP.replace(INLINE_PLACEHOLDER, hashes.join(" "));
                const meta = `<meta http-equiv="Content-Security-Policy" content="${policy}">`;
                await writeFile(file, html.replace(charset, (tag) => tag + meta));
                logger.info(`index.html CSP pins ${hashes.length} inline script hash(es)`);
            },
        },
    };
}
