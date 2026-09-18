# Vendored tomlc17

- Upstream: <https://github.com/cktan/tomlc17>
- Release: [R260821](https://github.com/cktan/tomlc17/releases/tag/R260821),
  published 2026-08-21.
- Commit: `e0e8868546b4611a86fcdb284819e60866a3480b`.
- Imported files: upstream `src/tomlc17.c` and `src/tomlc17.h`.
- License: MIT; see [LICENSE](LICENSE). The existing license notice retains
  the upstream repository URL in addition to the release's license text.

The release was compared with the original project import (`db02841`) and the
project hardening in `8bbf907` and `f6bb014` before integration. The header's
`TOMLC17_RELEASE_AFTER` is upstream's coarse version marker, not the release tag;
use the tag and commit above to identify this import.

## Retained project patches

The C source differs from R260821 in these places:

- `scan_copystr` initializes the destination even when no bytes remain.
- `page_create` computes its allocation size with standard `offsetof` plus the
  checked payload size. Taking a member address through a null page pointer
  triggered Linux UBSan even though the intended byte count was correct.
- `is_hex_char` uses explicit ASCII ranges rather than passing scanner sentinel
  values through character classification.
- `read_date` validates the fixed-width date incrementally, rejects short input
  before looking farther, and computes components without unbounded conversion.
- `cell_realloc` computes its growth margin in checked-width integer arithmetic
  and caps capacity at `INT_MAX`. The release's floating-point-to-`int` conversion
  overflowed for large file-buffer requests; an allocator-hook probe reproduced
  this under UBSan without allocating gigabytes.
- `ARRAY_MAX` is 36000 instead of upstream's 16384, preserving load support for
  the project's maximum-length experiment exports (`EXPERIMENT_MAX_FRAMES`).
  Larger arrays still fail explicitly.

Other earlier project fixes are retained by equivalent upstream implementations:

- All three `parse_val` callers initialize and free pending values on failure.
  Inline table/array insertion failures also free the unowned value. Dotted-key
  checks now happen before parsing a value, so those failures cannot strand it.
  This replaces the project's cleanup inside `parse_val` and its early-return
  sites while preserving the same ownership rule.
- `tab_add` uses one `tab_emplace` lookup and rejects a populated slot, preserving
  the project's removal of duplicate table scans. Partial allocation failures
  preserve every successful reallocation for teardown.
- Standard C `\033` replaces the old nonstandard `\e`, equivalent to the
  project's `\x1b` spelling. Numeric scanning uses checked integer accumulation
  and unsigned-character classification.

## Release behavior and integration

This import includes unordered table equivalence (arrays remain ordered), public
`TOML_FLAG_*` constants, type-correct array-of-table merging, independent merged
key/source ownership, source locations and named parsing, amortized container
growth, and paged string allocation. Tables are capped at 16384 entries and
arrays at 36000; the existing 30-level bracket/brace and 10-part key limits remain.

Existing parse/query/free entry points remain source-compatible. `toml_datum_t`
has additional source metadata, so rebuild all consumers together; this is not
binary-compatible with old objects. `toml_parse` now reads only the supplied byte
range and no longer requires a trailing NUL. Failed parse results remain safe
to free; all merge results must also be freed, including failed merges.

One leading UTF-8 BOM is accepted; repeated or misplaced BOMs outside strings or
quoted keys are rejected. Python level readers share
`tools/validate_levels.py::load_level`, which decodes files with `utf-8-sig`
for the same leading-BOM behavior, without normalizing line endings.
Level/profile schemas and serialized output are unchanged. The Python validator
still uses the installed standard-library TOML grammar; this update does not
replace it with a TOML 1.1 parser.

## Regression coverage

`tests/parser_boundary_test.c` retains the deterministic 10000-mutation corpus,
date/depth cases and allocation ledger. Additional cases cover bounded input,
BOM placement, unordered equivalence, mixed-array merge replacement, merged
ownership, and allocation failures across small/large string pages, container
growth and source-name copying. It runs as part of `level-serializer-test`.
`make parser-encoding-probe` checks encoding parity across the validator,
documentation, catalog and scripted-smoke readers. `make parser-allocation-probe`
compiles and runs `tests/parser_allocation_test.c`, a standalone white-box
allocation-limit probe that includes the vendored implementation directly.
Both are prerequisites of `make test`, in addition to its existing 15 native
regression binaries; `make sanitize` also runs them and instruments the C probe.
Existing serializer, profile and level-validator tests cover application
contracts.
