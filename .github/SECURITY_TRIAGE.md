# Native file-access security triage

The native game and editor are local desktop applications running with the invoking user's filesystem permissions. They intentionally open user-selected level/experiment files and support an explicit native profile path. They do not expose these operations as a network service or run them with elevated privileges.

The following CodeQL `cpp/path-injection` findings were reviewed against the native input boundaries. Restricting every selected file to the repository would remove supported Open, Save As, profile, and replay behavior rather than establish a missing privilege boundary.

| Alerts | Input and operation | Assessment |
| --- | --- | --- |
| #42 | `editor_main.c`: command-line level path passed to `editor_load_level` | Intentional local-user file selection; the loader rejects overlong paths before opening. |
| #61 | `editor_files.c`: path returned by the native Open dialog | Intentional file selection by the user. |
| #59, #60 | `editor_files.c`: native Save As destination | The user selects the destination; private editor paths are rejected, existing targets require confirmation, and checked replacement/create-only policies protect concurrent changes. |
| #56, #57, #58 | `game_profile.c`: native profile read, lock, and save paths | The path comes from the explicit `--profile` option or SDL's per-user preference directory, not a profile document field. Saves compare the baseline and use cooperating-process locking. |
| #75 | `app_session.c`: experiment path passed to `game_experiment_load` | The local user provides `--experiment PATH`, together with an explicit level. |
| #76 | `game_experiment.c`: fingerprint of the already selected level | The loader fingerprints `gs->level_path` and compares its content hash. It does not open the experiment document's `level_path` string. |

These are false positives for path-injection privilege escalation under the current native application contract. Reassess them if a network interface, privileged execution mode, automatic untrusted path selection, or a new file-access boundary is introduced. Keep CodeQL enabled; no query-wide suppression is added.

The profile key copies use bounded SDL copies after existing key validation, with tests at the 255-byte accepted and 256-byte rejected boundaries. Recovery timestamp formatting uses caller-owned `struct tm` storage through the platform's reentrant conversion API. These changes remove the unsafe primitives reported by alerts #62, #63, and #64 while preserving the existing file formats and UI behavior.
