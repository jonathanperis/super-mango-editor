import { defineConfig } from "astro/config";
import { satteri } from "@astrojs/markdown-satteri";
import sitemap from "@astrojs/sitemap";
import tailwindcss from "@tailwindcss/vite";
import { homeCsp } from "./integrations/home-csp.mjs";

export default defineConfig({
    integrations: [sitemap(), homeCsp()],
    output: "static",
    outDir: "out",
    // The custom domain serves this project at the host root. A repository
    // base path would make every asset 404 once GitHub Pages leaves
    // jonathanperis.github.io/super-mango-editor/.
    site: "https://super-mango-editor.jonathanperis.tech",
    base: "",
    markdown: {
        processor: satteri(),
    },
    vite: {
        plugins: [tailwindcss()],
    },
});
