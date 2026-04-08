import type { NextConfig } from "next";

const nextConfig: NextConfig = {
    output: "export",
    basePath: "/super-mango-editor",
    images: {
        unoptimized: true,
    },
};

export default nextConfig;
