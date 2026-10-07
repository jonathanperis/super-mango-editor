# Native file-access security triage

The native game and editor are local desktop applications running with the invoking user's filesystem permissions. They intentionally open user-selected level/experiment files and support an explicit native profile path. They do not expose these operations as a network service or run them with elevated privileges.

The following CodeQL `cpp/path-injection` findings were reviewed against the native input boundaries. Restricting every selected file to the repository would remove supported Open, Save As, profile, and replay behavior rather than establish a missing privilege boundary.

| Alerts | Input and operation | Assessment |
| --- | --- | --- |
| #42 | `editor_main.c`: command-line level path passed to `editor_load_level` | Intentional local-user file selection; the loader rejects overlong paths before opening. |
| #61 | `editor_files.c`: path returned by the native Open dialog | Intentional file selection by the user. |
| #59, #60 | `editor_files.c`: native Save As destination | The user selects the destination; private editor paths are rejected, existing targets require confirmation, and checked replacement/create-only policies protect concurrent changes. |
| #56, #57, #58 | `game_profile.c`: native profile read, lock, and save paths | The path comes from the explicit `--profile` option or the OS per-user preference directory, not a profile document field. Saves compare the baseline and use cooperating-process locking. |
| #75 | `app_session.c`: experiment path passed to `game_experiment_load` | The local user provides `--experiment PATH`, together with an explicit level. |
| #76 | `game_experiment.c`: fingerprint of the already selected level | The loader fingerprints `gs->level_path` and compares its content hash. It does not open the experiment document's `level_path` string. |

These are false positives for path-injection privilege escalation under the current native application contract. Reassess them if a network interface, privileged execution mode, automatic untrusted path selection, or a new file-access boundary is introduced. Keep CodeQL enabled; no query-wide suppression is added.

The profile key copies use bounded copies after existing key validation, with tests at the 255-byte accepted and 256-byte rejected boundaries. Recovery timestamp formatting uses caller-owned `struct tm` storage through the platform's reentrant conversion API. These changes remove the unsafe primitives reported by alerts #62, #63, and #64 while preserving the existing file formats and UI behavior.

## Raylib migration findings (PR #304)

Replacing opaque backend preference-path helpers with project-owned code makes
the existing local-user path flows visible to CodeQL. The following findings
were reviewed against the same desktop permission boundary:

| Alerts | Input and operation | Assessment |
| --- | --- | --- |
| #83 | `app_session.c`: explicit experiment path | Same user-selected replay contract as #75; experiment content does not select another file to open. |
| #84, #85 | `editor_main.c`: command-line level, normal or smoke mode | Intentional file selection; both routes reject overlong paths before opening. Smoke does not discover personal recovery/recent data. |
| #86–#93 | `editor_files.c`: playtest, recent files, autosave and recovery below the preference root | The invoking user's `HOME`/`XDG_DATA_HOME` determines the private root. Fixed application suffixes and generated filenames select these files; level documents cannot redirect that root. |
| #94–#102 | `editor_files.c`: selected-document fingerprints and saves | The document path originates from local selection. Existing baseline checks, create-only saves and explicit replacement decisions retain concurrent-write protection. |

These 20 path-injection findings were dismissed as false positives after explicit
maintainer authorization and saved-state verification. No query or check is disabled.

Alert #103 identifies the playtest path copy. Its preceding length check already
rejects insufficient capacity; the copy now also takes the explicit destination
capacity through `str_copy`, preserving failure-before-save and full-path copying.
Alert #104 prompted documentation of the debug overlay's collision/interaction
coordinate conventions. The learning/readability follow-up addresses #81 with
one early return per supported legacy binding range. A differential check retains
the same accepted IDs, rejected holes and integer extremes.

Alert #82 compares zoom values selected from the exactly representable set
`{1, 2, 3, 5}`, not accumulated floating-point estimates. It was dismissed as a
false positive after separate explicit maintainer authorization.
Initialization, toolbar selection and wheel input are the only production zoom
assignments. Their review threads were resolved after this contract check.

After token-preserving formatting, CodeQL reissued the zoom finding as `#105`
and the previously reviewed command-line editor path (`#85`) as `#106`.
Both replacement IDs were separately authorized for dismissal with the same
verified rationale. The PR alert readback reports no open scanning alerts.

## Audit remediation findings (PR #306)

The audit remediation moved and added local file operations, and the C/C++
analysis reissued the same path-injection class at new locations. Each was
reviewed against the desktop permission boundary above:

| Alerts | Input and operation | Assessment |
| --- | --- | --- |
| #130 | `editor_files.c`: `editor_load_level` reads the symlink-resolved target of the user-selected document | Same intentional selection as #61/#84. Resolving once and reading that exact file is what lets plain Save refuse a link repointed after load. |
| #127, #128, #129 | `editor_files.c`: plain Save fingerprints and replaces/creates the document's write target | Same as #94–#102. The target is the opened document, or the file behind its link only while the link still names the file this session read; Save As refuses symlinked destinations; private editor paths are rejected on the path actually written. |
| #126 | `editor_files.c`: recovery snapshot below the preference root | Same as #86–#93: `HOME`/`XDG_DATA_HOME` select the private root, fixed suffixes and generated names select the file. |
| #125 | `editor_files.c`: atomic recent-files temp file beside the recent list | Same preference root as #126; the temp name is the list path plus a pid suffix, created exclusively and renamed over the list. |
| #131 | `serializer_io.c`: `open()` of the parent directory to fsync it after a save | Opens (read-only) only the directory of a file the same save just wrote; it adds no new path source. |

Alert #122 is the zoom-preset equality check reissued after the editor event
code moved; it is the same exactly-representable `{1, 2, 3, 5}` comparison as
#82/#105. The two `cpp/poorly-documented-function` warnings (#123, #124) were
fixed by documenting `editor_clamp_placement` and `editor_entity_array`.

These eight findings (#122, #125–#131) were dismissed as false positives
after explicit maintainer authorization on 2026-10-06, as in the earlier
rounds. No query or check is disabled.

## Editor recovery split (PR #318)

Moving autosave and crash recovery out of `editor_files.c` into
`editor_recovery.c` reissued the path-injection class at the moved lines. No
path source or file operation changed; the code is the same as the reviewed
#86–#93 and #126:

| Alerts | Input and operation | Assessment |
| --- | --- | --- |
| #150 | `editor_recovery.c`: temp file for a recovery entry's metadata, beside that metadata | Same as #125/#126: `HOME`/`XDG_DATA_HOME` select the private preference root; fixed suffixes and generated ids select the file, and the temp is created exclusively. |
| #149 | `editor_recovery.c`: reading a recovery entry's metadata | Same as #86–#93: the metadata path is built from the private root and a generated id, not from level data. |
| #148 | `editor_recovery.c`: writing the autosave snapshot | Same as #126: the snapshot path is below the private root with a generated name. |
| #147 | `editor_recovery.c`: loading a recovery snapshot the user chose to restore | Same as #86–#93: the snapshot path comes from validated metadata below the private root. |

These four findings (#147–#150) were dismissed as false positives after
explicit maintainer authorization on 2026-10-07. No query or check is disabled.

## Create-only fallback without hard links (PR #315)

The editor fix that lets a create-only save work on drives without hard
links (FAT/exFAT, some network shares) added `open()` calls inside
`serializer_create_without_link`. The game profile's first save goes through
that create-only path, so the analysis followed the profile path into it:

| Alerts | Input and operation | Assessment |
| --- | --- | --- |
| #145, #146 | `game_profile.c`: first save of the player profile, temp file and target, through the create-only fallback | Same as #56–#58: the profile path comes from the explicit native `--profile` flag or the invoking user's per-user preference root (`HOME`/`XDG_DATA_HOME`), never from profile file contents. The fallback claims the target with `O_EXCL` and writes through that descriptor, so it never replaces an existing file. |

These two findings (#145, #146) were dismissed as false positives after
explicit maintainer authorization on 2026-10-07. No query or check is disabled.
