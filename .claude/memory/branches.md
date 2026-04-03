---
name: Branch conventions
description: Active branches and what each is for
type: project
---

| Branch | Purpose | Status |
|--------|---------|--------|
| `main` | Stable, shippable code | Active |
| `feature/overall-improvements` | Overlay screens, start menu play-through, code structure/optimization | Active (2026-04-02) |

## Convention

- Feature branches: `feature/<short-description>`
- Fix branches: `fix/<short-description>`
- All work for the 2026-04-02 improvement session happens on `feature/overall-improvements`
- PRs merge to `main` via `gh pr create`
