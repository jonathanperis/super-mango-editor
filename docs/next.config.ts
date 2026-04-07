import type { NextConfig } from "next";

const nextConfig: NextConfig = {
    output: "export",
    basePath: "/super-mango-editor",
    images: {
        unoptimized: true,
    },
    headers: async () => [
        {
            source: "/:all*(svg|jpg|jpeg|png|gif|ico|webp|woff|woff2)",
            headers: [
                { key: "Cache-Control", value: "public, max-age=31536000, immutable" },
            ],
        },
        {
            source: "/:all*(js|css)",
            headers: [
                { key: "Cache-Control", value: "public, max-age=31536000, immutable" },
            ],
        },
    ],
};

export default nextConfig;
