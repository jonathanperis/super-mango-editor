/*
 * Code shown on the site is cut from the real C source at build time (imported
 * with `?raw`), so the pages always show the code that actually ships. A
 * missing marker fails the build instead of silently showing stale code.
 *
 * The excerpt starts at the line containing `startMarker`, stops just before
 * `endMarker`, and has its common indentation removed.
 */
export function sourceExcerpt(file: string, source: string, startMarker: string, endMarker: string): string {
    const marker = source.indexOf(startMarker);
    const end = marker < 0 ? -1 : source.indexOf(endMarker, marker);
    if (marker < 0 || end < 0) {
        throw new Error(`${file} excerpt markers not found: "${startMarker}" .. "${endMarker}"`);
    }
    const start = source.lastIndexOf('\n', marker) + 1;   /* include the indent */
    const lines = source.slice(start, end).replace(/\s+$/, '').split('\n');
    const indent = Math.min(...lines.filter(l => l.trim()).map(l => l.match(/^ */)![0].length));
    return lines.map(l => l.slice(indent)).join('\n');
}
