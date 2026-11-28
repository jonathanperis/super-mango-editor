import { defineConfig } from "astro/config";
import { satteri } from "@astrojs/markdown-satteri";
import sitemap from "@astrojs/sitemap";
import tailwindcss from "@tailwindcss/vite";
import { homeCsp } from "./integrations/home-csp.mjs";

const isProd = process.env.NODE_ENV === "production";

export default defineConfig({
    integrations: [sitemap(), homeCsp()],
    output: "static",
    outDir: "out",
    // GitHub Pages serves this project under the account's custom domain at
    // https://jonathanperis.dev/super-mango-editor/, so production builds use
    // the repository name as the base path. Development stays at "/".
    site: "https://jonathanperis.dev",
    base: isProd ? "/super-mango-editor" : "",
    markdown: {
        processor: satteri(),
    },
    vite: {
        plugins: [tailwindcss()],
    },
});
