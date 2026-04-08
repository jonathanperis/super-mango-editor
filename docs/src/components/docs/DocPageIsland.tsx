"use client";

import { useState, useCallback, useEffect } from "react";
import { useScrollspy } from "@/hooks/useScrollspy";
import Sidebar from "./Sidebar";

const SECTION_LABELS: Record<string, string> = {
    home: "Home",
    /* Engine & Code */
    architecture: "Architecture",
    "source-files": "Source Files",
    "player-module": "Player Module",
    "constants-reference": "Constants Reference",
    /* Content & Assets */
    "entities-hazards": "Entities & Hazards",
    "collectibles-surfaces": "Collectibles & Surfaces",
    assets: "Assets",
    sounds: "Sounds",
    /* Building & Contributing */
    "build-system": "Build System",
    "level-design": "Level Design",
    "level-editor": "Level Editor",
    "developer-guide": "Developer Guide",
};

export const SECTION_CATEGORIES: { label: string | null; ids: string[] }[] = [
    { label: null, ids: ["home"] },
    {
        label: "Engine & Code",
        ids: ["architecture", "source-files", "player-module", "constants-reference"],
    },
    {
        label: "Content & Assets",
        ids: ["entities-hazards", "collectibles-surfaces", "assets", "sounds"],
    },
    {
        label: "Building & Contributing",
        ids: ["build-system", "level-design", "level-editor", "developer-guide"],
    },
];

const SECTION_IDS = SECTION_CATEGORIES.flatMap((c) => c.ids);

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
                categories={SECTION_CATEGORIES}
                sectionLabels={SECTION_LABELS}
                activeSection={activeSection}
            />
        </>
    );
}
