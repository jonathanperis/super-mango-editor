"use client";

import { useState, useCallback, useEffect } from "react";
import { useScrollspy } from "@/hooks/useScrollspy";
import Sidebar from "./Sidebar";

const SECTION_LABELS: Record<string, string> = {
    home: "Home",
    architecture: "Architecture",
    assets: "Assets",
    "build-system": "Build System",
    "constants-reference": "Constants Reference",
    "developer-guide": "Developer Guide",
    "player-module": "Player Module",
    sounds: "Sounds",
    "source-files": "Source Files",
};

const SECTION_IDS = Object.keys(SECTION_LABELS);

export default function DocPageIsland() {
    const [menuOpen, setMenuOpen] = useState(false);
    const [searchQuery, setSearchQuery] = useState("");
    const { activeSection } = useScrollspy(SECTION_IDS);
    const toggleMenu = useCallback(() => setMenuOpen((p) => !p), []);

    /* Apply search filtering by toggling hidden-section class on DOM sections */
    useEffect(() => {
        if (!searchQuery) {
            SECTION_IDS.forEach((id) => {
                document.getElementById(id)?.classList.remove("hidden-section");
            });
            return;
        }
        const q = searchQuery.toLowerCase();
        SECTION_IDS.forEach((id) => {
            const el = document.getElementById(id);
            if (!el) return;
            const visible = el.textContent?.toLowerCase().includes(q) ?? true;
            el.classList.toggle("hidden-section", !visible);
        });
    }, [searchQuery]);

    return (
        <>
            <div className="mobile-header">
                <button className="menu-toggle" onClick={toggleMenu} aria-label="Toggle Menu">
                    <span /><span /><span />
                </button>
                <div className="repo-title">Super Mango Editor</div>
            </div>
            <div
                className={`overlay${menuOpen ? " open" : ""}`}
                onClick={toggleMenu}
            />
            <Sidebar
                menuOpen={menuOpen}
                toggleMenu={toggleMenu}
                searchQuery={searchQuery}
                onSearchChange={setSearchQuery}
                sectionIds={SECTION_IDS}
                sectionLabels={SECTION_LABELS}
                activeSection={activeSection}
            />
        </>
    );
}
