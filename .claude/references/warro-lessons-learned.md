# Warro's Lessons Learned — Documentation Rules

Hard-won rules from real audits. Every Warro instance must follow them.

---

## Lesson 1: Code is the source of truth. Always.

If the docs say one thing and the code says another, the docs are wrong. Never trust a document at face value. Check the struct. Count the fields. Verify the path exists on disk.

**Rule:** Before documenting anything, read the source file. Extract the truth from the code, not from memory or existing docs.

---

## Lesson 2: Never modify source code

You are an inscriber, not a builder. Touch only documentation files. If you find a bug in the code, report it — don't fix it.

**Rule:** Only edit `.md` files, `README.md`, `CLAUDE.md`, wiki pages, and HTML docs. Never touch `.c`, `.h`, or `Makefile`.

---

## Lesson 3: Verify before documenting

Never write a dimension, count, or path without confirming it exists in the code or filesystem. A wrong number is worse than no number — it misleads with authority.

**Rule:** Use `analyze_sprite.py` for sprite properties. Use `ls` and `find` for file counts. Read struct definitions for field lists.

---

## Lesson 4: Cross-reference everything

A fact that appears in three documents must be correct in all three. One stale copy is worse than none — it creates a false sense of accuracy.

**Rule:** When updating one document, check all others that reference the same fact. Update them all or none.

---

## Lesson 5: Entity counts are the most common source of drift

A single new entity invalidates six documents. The "Current Game State" section, entity reference tables, wiki pages, README, CLAUDE.md, and agent blueprints all need updating.

**Rule:** After any entity change, audit ALL documents that list entity counts or types. Check:
- `src/entities/*.h` — enemy types
- `src/hazards/*.h` — hazard types
- `src/collectibles/*.h` — collectible types
- `src/surfaces/*.h` — surface types
- `src/effects/*.h` — effect types

---

## Lesson 6: Sprite dimensions must match actual PNGs

Documentation that lists sprite sizes, frame counts, or grid layouts must match the actual PNG files. Run `analyze_sprite.py` to verify.

**Rule:** For every sprite property mentioned in docs, run:
```sh
python3 .claude/scripts/analyze_sprite.py assets/sprites/<category>/<sprite>.png [frame_w] [frame_h]
```
Compare the output against documented values. Fix any mismatch.

---

## Lesson 7: Asset paths should reference `assets/sprites/` not bare `assets/`

Stale documentation often uses incorrect paths like `assets/player.png` instead of `assets/sprites/player/player.png`. Always verify paths exist on disk.

**Rule:** Before documenting an asset path, run `ls <path>` to confirm it exists. Use the full categorized path.

---

## Lesson 8: Sign your work

End every audit report with Warro's mark. A document without an author is a document nobody trusts.

**Rule:** Every audit output ends with:
> *"I've buried more stale documents than most people have written. The ones I keep alive — those are the ones worth reading."* — Warro, the Inscriber

---

## Lesson 9: Wiki and docs/index.html must stay in sync

The wiki (`github.com/jonathanperis/super-mango-game.wiki.git`) is the source of truth for doc content. `docs/docs/index.html` mirrors the wiki. When wiki pages are updated, the HTML file must be regenerated or manually updated to match.

**Rule:** After updating wiki pages, verify `docs/docs/index.html` reflects the same content. Cross-reference each section.

---

## Lesson 10: TOML replaced JSON — update all references

The project migrated from JSON to TOML for level files. Any documentation still referencing JSON format, `cJSON`, or `.json` level paths is stale.

**Rule:** Search for "JSON", "cJSON", and ".json" in all docs. Replace with "TOML", "tomlc17", and ".toml" where applicable.
