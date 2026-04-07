import type { Metadata } from "next";
import { Pixelify_Sans, DM_Mono } from "next/font/google";
import Analytics from "./components/analytics";
import JsonLd from "./components/json-ld";
import "./globals.css";

const pixelifySans = Pixelify_Sans({
    subsets: ["latin"],
    weight: ["400", "600", "700"],
    variable: "--font-pixel",
    display: "swap",
});

const dmMono = DM_Mono({
    subsets: ["latin"],
    weight: ["400", "500"],
    variable: "--font-mono",
    display: "swap",
});

export const metadata: Metadata = {
    title: "Super Mango - 2D Pixel Art Platformer",
    description:
        "A 2D pixel art platformer written in C11 + SDL2. Play in your browser via WebAssembly or download for macOS, Linux, and Windows.",
    keywords: [
        "Super Mango",
        "platformer",
        "2D game",
        "pixel art",
        "SDL2",
        "C11",
        "WebAssembly",
        "indie game",
        "side-scroller",
        "open source",
        "game development",
    ],
    metadataBase: new URL("https://jonathanperis.github.io/super-mango-editor"),
    alternates: {
        canonical: "/",
    },
    openGraph: {
        type: "website",
        title: "Super Mango - 2D Pixel Art Platformer",
        description:
            "A 2D pixel art platformer written in C11 + SDL2. Play in your browser via WebAssembly or download for macOS, Linux, and Windows.",
        url: "/",
        siteName: "Super Mango",
        locale: "en_US",
    },
    twitter: {
        card: "summary",
        title: "Super Mango - 2D Pixel Art Platformer",
        description:
            "A 2D pixel art platformer written in C11 + SDL2. Play in your browser via WebAssembly.",
        creator: "@jperis_silva",
    },
    other: {
        "theme-color": "#0f0e17",
    },
};

export default function RootLayout({
    children,
}: Readonly<{
    children: React.ReactNode;
}>) {
    return (
        <html lang="en" className={`${pixelifySans.variable} ${dmMono.variable}`}>
            <head>
                <link rel="dns-prefetch" href="https://www.googletagmanager.com" />
                <JsonLd />
            </head>
            <body>
                <Analytics />
                {children}
            </body>
        </html>
    );
}
