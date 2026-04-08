interface SidebarProps {
    menuOpen: boolean;
    toggleMenu: () => void;
    searchQuery: string;
    onSearchChange: (v: string) => void;
    sectionIds: string[];
    sectionLabels: Record<string, string>;
    activeSection: string;
}

export default function Sidebar({
    menuOpen,
    toggleMenu,
    searchQuery,
    onSearchChange,
    sectionIds,
    sectionLabels,
    activeSection,
}: SidebarProps) {
    const base = import.meta.env?.BASE_URL ?? "";
    return (
        <aside className={`sidebar${menuOpen ? " open" : ""}`}>
            <div className="sidebar-header">
                <h1 className="repo-title">Super Mango Editor</h1>
                <p className="repo-description">
                    2D side-scrolling platformer written in C using SDL2 — browser-playable via WebAssembly
                </p>
                <input
                    type="text"
                    className="search-box"
                    placeholder="Search documentation..."
                    value={searchQuery}
                    onChange={(e) => onSearchChange(e.target.value)}
                />
            </div>
            <div className="sidebar-nav-container">
                <ul>
                    {sectionIds.map((id) => (
                        <li key={id}>
                            <a
                                href={`#${id}`}
                                className={`nav-item${activeSection === id ? " active" : ""}`}
                                onClick={() => {
                                    if (window.innerWidth <= 768) toggleMenu();
                                }}
                            >
                                {sectionLabels[id]}
                            </a>
                        </li>
                    ))}
                </ul>
            </div>
            <div className="sidebar-footer">
                <a href={base} className="back-home">&#8592; Back to home</a>
                <a href="https://github.com/jonathanperis/super-mango-editor" target="_blank" rel="noopener noreferrer">View on GitHub</a>
                <div>Built by Jonathan Peris</div>
            </div>
        </aside>
    );
}
