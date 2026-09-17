#!/usr/bin/env python3
"""Check the built Pages site without a browser or network access."""

from __future__ import annotations

import re
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urljoin, urlsplit
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parents[1]
SITE = "https://jonathanperis.github.io/super-mango-editor/"
OUT = ROOT / "docs/out"


class Page(HTMLParser):
    def __init__(self, text: str):
        super().__init__()
        self.ids: set[str] = set()
        self.links: list[str] = []
        self.meta: dict[str, str] = {}
        self.canonical = ""
        self.text: list[str] = []
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = {key: value or "" for key, value in attrs}
        if attrs.get("id"):
            self.ids.add(attrs["id"])
        for key in ("href", "src"):
            if attrs.get(key):
                self.links.append(attrs[key])
        if tag == "meta":
            self.meta[attrs.get("name") or attrs.get("property", "")] = attrs.get("content", "")
        if tag == "link" and attrs.get("rel") == "canonical":
            self.canonical = attrs.get("href", "")

    def handle_data(self, data):
        self.text.append(data)


def check_site(out: Path) -> list[str]:
    failures: list[str] = []
    # Standalone Emscripten shells are added by deployment, not Astro routes.
    pages = {path: Page(path.read_text(encoding="utf-8")) for path in sorted(out.rglob("index.html"))}
    if not pages:
        return [f"{out}: no built pages; run bun run build first"]

    def local_file(url: str) -> Path | None:
        parsed = urlsplit(url)
        if parsed.scheme not in ("http", "https") or parsed.netloc != urlsplit(SITE).netloc:
            return None
        prefix = urlsplit(SITE).path
        if parsed.path == prefix.rstrip("/"):
            return out / "index.html"
        if not parsed.path.startswith(prefix):
            failures.append(f"URL outside project base: {url}")
            return None
        target = out / unquote(parsed.path[len(prefix):])
        return target / "index.html" if target.is_dir() else target

    descriptions: set[str] = set()
    for path, page in pages.items():
        relative = path.relative_to(out).as_posix()
        url = urljoin(SITE, relative.removesuffix("index.html"))
        if page.canonical != url or page.meta.get("og:url") != url:
            failures.append(f"{relative}: canonical/og:url must be {url}")
        description = page.meta.get("description", "")
        if not description or description in descriptions:
            failures.append(f"{relative}: missing or duplicate page description")
        descriptions.add(description)
        for key in ("og:description", "twitter:description"):
            if page.meta.get(key) != description:
                failures.append(f"{relative}: {key} differs from page description")
        for link in page.links:
            target_url = urljoin(url, link)
            target = local_file(target_url)
            if target is None:
                continue
            if not target.is_file():
                failures.append(f"{relative}: missing target {link}")
            fragment = unquote(urlsplit(target_url).fragment)
            if fragment and target in pages and fragment not in pages[target].ids:
                failures.append(f"{relative}: missing anchor {link}")

    expected_urls: set[str] = {SITE}
    for source in sorted((ROOT / "docs/wiki").glob("*.md")):
        route = "docs/" if source.stem == "index" else f"docs/{source.stem}/"
        expected_urls.add(urljoin(SITE, route))
        page = pages.get(out / route / "index.html")
        heading = re.search(r"^# (.+)$", source.read_text(encoding="utf-8"), re.M)
        if page is None or (heading and heading.group(1) not in "".join(page.text)):
            failures.append(f"{source.name}: Markdown page content is not published at {route}")

    robots = out / "robots.txt"
    sitemap_urls = re.findall(r"^Sitemap:\s*(\S+)", robots.read_text(encoding="utf-8"), re.M) if robots.exists() else []
    if not sitemap_urls:
        failures.append("robots.txt: missing sitemap declaration")
    indexed_urls: set[str] = set()
    visited: set[str] = set()

    def check_sitemap(url: str):
        if url in visited:
            return
        visited.add(url)
        path = local_file(url)
        if path is None or not path.is_file():
            failures.append(f"missing sitemap: {url}")
            return
        tree = ElementTree.fromstring(path.read_text(encoding="utf-8"))
        locations = [node.text for node in tree.findall(".//{*}loc") if node.text]
        if tree.tag.endswith("sitemapindex"):
            for location in locations:
                check_sitemap(location)
        else:
            indexed_urls.update(locations)

    for sitemap in sitemap_urls:
        check_sitemap(sitemap)
    if indexed_urls != expected_urls:
        failures.append(f"sitemap route mismatch: missing={sorted(expected_urls - indexed_urls)}, extra={sorted(indexed_urls - expected_urls)}")
    return failures


def main() -> int:
    failures = check_site(OUT)
    if failures:
        print("built docs check failed:\n" + "\n".join(f"- {item}" for item in failures))
        return 1
    print("built docs check: ok (routes, content, links, anchors, metadata, sitemap)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
