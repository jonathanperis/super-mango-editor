/*
 * Content collections for the docs site.
 *
 * `docs` is the Builder Manual: every Markdown page in ../wiki/ becomes one
 * collection entry, with its id taken from the file name (index.md -> "index").
 * The pages stay in docs/wiki/ because the repository's generators and drift
 * checks write and read them there (level-catalog.md, overlay-snapshots.md).
 *
 * Navigation order, labels and descriptions live in src/lib/docsSidebar.ts so
 * the sidebar, hub cards and routes share one list; the route checks that each
 * listed page has an entry. Frontmatter is optional. When present it may only
 * override the page title or description, and any other key is a build error
 * instead of being silently ignored.
 */
import { defineCollection } from 'astro:content';
import { glob } from 'astro/loaders';
import { z } from 'astro/zod';

const docs = defineCollection({
    loader: glob({ pattern: '*.md', base: './wiki' }),
    schema: z
        .object({
            title: z.string().min(1).optional(),
            description: z.string().min(1).optional(),
        })
        .strict(),
});

export const collections = { docs };
