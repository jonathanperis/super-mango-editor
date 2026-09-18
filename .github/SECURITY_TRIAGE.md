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
coordinate conventions. The two advisory findings are intentional: #81 enumerates
the supported legacy binding ranges; #82 compares zoom values selected from the
exactly representable set `{1, 2, 3, 5}`, not accumulated floating-point estimates.
Initialization, toolbar selection and wheel input are the only production zoom
assignments. Their review threads were resolved after this contract check; the
two advisory scan alerts remain open.
