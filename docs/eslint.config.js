export default [
    {
        ignores: [
            "node_modules/**",
            "out/**",
            "src/**/*.astro",
            "src/**/*.ts",
            "src/**/*.tsx",
        ],
    },
    {
        files: ["**/*.{js,mjs}"],
        languageOptions: {
            ecmaVersion: "latest",
            sourceType: "module",
            globals: {
                process: "readonly",
            },
        },
    },
];
