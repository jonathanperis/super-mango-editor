---
name: Per-frame texture allocation issue
description: Two places in the codebase create+destroy SDL textures every frame — known optimization target
type: project
---

## Rule

Do NOT create `SDL_Texture*` or `SDL_Surface*` inside a render loop that runs every frame for static strings. Pre-cache text textures at init time.

**Why:** SDL_CreateTextureFromSurface + TTF_RenderText_Solid + SDL_FreeSurface + SDL_DestroyTexture every frame is unnecessary GPU/CPU work for strings that never change. It also fragments memory over time.

## Affected locations

1. `src/screens/start_menu.c:95-156` — three strings ("Super Mango", "Press ENTER to play", etc.) recreated every frame
2. `src/game.c:1476-1495` — "A inicializar controle..." HUD message recreated every frame while gamepad is initializing

## How to apply

Pre-render text to `SDL_Texture*` once at init time, store in the struct, reuse every frame.
Pattern:
```c
/* At init: */
SDL_Surface *surf = TTF_RenderText_Solid(font, "My text", color);
SDL_Texture *tex  = SDL_CreateTextureFromSurface(renderer, surf);
SDL_FreeSurface(surf);   /* surface no longer needed after upload */

/* At render: */
SDL_RenderCopy(renderer, tex, NULL, &dst);

/* At cleanup: */
SDL_DestroyTexture(tex); tex = NULL;
```
