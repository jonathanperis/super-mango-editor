import { defineConfig } from "astro/config";
import react from "@astrojs/react";
import mdx from "@astrojs/mdx";
import sitemap from "@astrojs/sitemap";
import tailwindcss from "@tailwindcss/vite";

const isProd = process.env.NODE_ENV === "production";

export default defineConfig({
    integrations: [react(), mdx(), sitemap()],
    output: "static",
    outDir: "out",
    site: "https://jonathanperis.github.io",
    base: isProd ? "/super-mango-editor" : "",
    vite: {
        plugins: [tailwindcss()],
    },
});
