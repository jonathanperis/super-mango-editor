"use client";
import { useState, useCallback } from "react";
import { useScrollspy } from "@/hooks/useScrollspy";
import Sidebar from "@/app/components/docs/Sidebar";
import HomeSection from "@/app/components/docs/sections/HomeSection";
import ArchitectureSection from "@/app/components/docs/sections/ArchitectureSection";
import AssetsSection from "@/app/components/docs/sections/AssetsSection";
import BuildSystemSection from "@/app/components/docs/sections/BuildSystemSection";
import ConstantsSection from "@/app/components/docs/sections/ConstantsSection";
import DeveloperGuideSection from "@/app/components/docs/sections/DeveloperGuideSection";
import PlayerModuleSection from "@/app/components/docs/sections/PlayerModuleSection";
import SoundsSection from "@/app/components/docs/sections/SoundsSection";
import SourceFilesSection from "@/app/components/docs/sections/SourceFilesSection";

const SECTION_IDS = ["home", "architecture", "assets", "build-system", "constants-reference", "developer-guide", "player-module", "sounds", "source-files"];
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

export default function DocsPage() {
  const [menuOpen, setMenuOpen] = useState(false);
  const [searchQuery, setSearchQuery] = useState("");
  const { activeSection } = useScrollspy(SECTION_IDS);
  const toggleMenu = useCallback(() => setMenuOpen((p) => !p), []);

  const isSectionVisible = (id: string) => {
    if (!searchQuery) return true;
    const el = document.getElementById(id);
    return el?.textContent?.toLowerCase().includes(searchQuery.toLowerCase()) ?? true;
  };

  return (
    <div className="docs-layout">
      <div className="mobile-header">
        <button className="menu-toggle" onClick={toggleMenu} aria-label="Toggle Menu">
          <span /><span /><span />
        </button>
        <div className="repo-title">Super Mango Editor</div>
      </div>
      <div className={`overlay${menuOpen ? " open" : ""}`} onClick={toggleMenu} />
      <Sidebar
        menuOpen={menuOpen}
        toggleMenu={toggleMenu}
        searchQuery={searchQuery}
        onSearchChange={setSearchQuery}
        sectionIds={SECTION_IDS}
        sectionLabels={SECTION_LABELS}
        activeSection={activeSection}
        isSectionVisible={isSectionVisible}
      />
      <div className="content-wrapper">
        <main className="content" id="mainContent">
          <HomeSection visible={isSectionVisible("home")} />
          <ArchitectureSection visible={isSectionVisible("architecture")} />
          <AssetsSection visible={isSectionVisible("assets")} />
          <BuildSystemSection visible={isSectionVisible("build-system")} />
          <ConstantsSection visible={isSectionVisible("constants-reference")} />
          <DeveloperGuideSection visible={isSectionVisible("developer-guide")} />
          <PlayerModuleSection visible={isSectionVisible("player-module")} />
          <SoundsSection visible={isSectionVisible("sounds")} />
          <SourceFilesSection visible={isSectionVisible("source-files")} />
        </main>
      </div>
    </div>
  );
}
