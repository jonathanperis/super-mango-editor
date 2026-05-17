# Docs

Astro static site deployed to GitHub Pages.

## Commands

Run from this directory (`docs/`):

| Command | Action |
|---|---|
| `bun install` | Install dependencies |
| `bun run dev` | Start dev server |
| `bun run build` | Build to `./out/` |
| `bun run preview` | Preview production build locally |
| `bun run lint` | Run `astro check` |
| `bun run drift` | Run the repository semantic docs drift gate (`make docs-drift`) |

## Environment

Copy `.env.example` to `.env` and fill in local values when needed. Production Pages builds set `PUBLIC_GA_ID` from the repository secret currently named `NEXT_PUBLIC_GA_ID`.

| Variable | Description |
|---|---|
| `PUBLIC_GA_ID` | Optional Google Analytics 4 Measurement ID |
