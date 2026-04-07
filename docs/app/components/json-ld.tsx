export default function JsonLd() {
    const data = {
        "@context": "https://schema.org",
        "@type": "VideoGame",
        name: "Super Mango",
        description:
            "A 2D pixel art platformer written in C11 + SDL2. Play in your browser via WebAssembly or download for macOS, Linux, and Windows.",
        url: "https://jonathanperis.github.io/super-mango-editor/",
        genre: ["Platformer", "Indie", "Pixel Art"],
        gamePlatform: ["Web Browser", "macOS", "Linux", "Windows"],
        applicationCategory: "Game",
        operatingSystem: ["macOS", "Linux", "Windows", "Web"],
        programmingLanguage: "C11",
        author: [
            {
                "@type": "Person",
                name: "Jonathan Peris",
                url: "https://github.com/jonathanperis",
            },
            {
                "@type": "Person",
                name: "Fernando Santos",
                url: "https://github.com/fersantos",
            },
        ],
        isAccessibleForFree: true,
        license:
            "https://github.com/jonathanperis/super-mango-editor/blob/master/LICENSE",
        codeRepository:
            "https://github.com/jonathanperis/super-mango-editor",
    };

    return (
        <script
            type="application/ld+json"
            dangerouslySetInnerHTML={{ __html: JSON.stringify(data) }}
        />
    );
}
