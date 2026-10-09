---
title: Debugging C
description: Find crashes and memory bugs with sanitizers, lldb or gdb, and a quick profiling pass.
---

# Debugging C

C does not stop you from reading freed memory or writing past the end of an
array. Often the program even keeps running, and the damage shows up much later
somewhere unrelated. This page shows the tools we use to catch those mistakes
close to where they happen, using three tiny planted bugs and then the real game.

All the output below is real: it was captured on macOS (Apple clang, arm64).
Addresses, process ids and line numbers will differ on your machine. On Linux
the reports have the same shape; gdb words things a little differently from
lldb, and we show its commands beside lldb's.

On this page:

- [The practice files](#the-practice-files)
- [AddressSanitizer: writing past an array](#addresssanitizer-writing-past-an-array)
- [AddressSanitizer: using a texture after freeing it](#addresssanitizer-using-a-texture-after-freeing-it)
- [UndefinedBehaviorSanitizer: signed overflow](#undefinedbehaviorsanitizer-signed-overflow)
- [A crash in the debugger](#a-crash-in-the-debugger)
- [Stepping through the game](#stepping-through-the-game)
- [Sanitizers on the whole game](#sanitizers-on-the-whole-game)
- [A quick profiling pass](#a-quick-profiling-pass)

## The practice files

The planted bugs live in `labs/debugging/`. They are small, self-contained C
files that copy one idea from the game each. They are **not** part of the game
or editor build (the Makefile only compiles `src/`), so we compile them by hand
from the repository root:

```sh
mkdir -p out/labs
cc -std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer \
   labs/debugging/coin_overflow.c -o out/labs/coin_overflow
cc -std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer \
   labs/debugging/texture_after_free.c -o out/labs/texture_after_free
cc -std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer \
   labs/debugging/score_overflow.c -o out/labs/score_overflow
# A plain build, without sanitizers, for the debugger section:
cc -std=c11 -g -O0 labs/debugging/texture_after_free.c -o out/labs/texture_after_free_plain
```

What the flags do:

- `-g` keeps file names, line numbers and variable names in the program, so
  reports and the debugger can point at source lines.
- `-O0` turns optimisation off. Variables stay where you expect them and lines
  run in order, which makes the debugger much easier to follow.
- `-fsanitize=address,undefined` adds AddressSanitizer (ASan) and
  UndefinedBehaviorSanitizer (UBSan) checks around memory accesses and risky
  arithmetic. The program gets slower and bigger; that is fine for practice.
- `-fno-omit-frame-pointer` keeps the frame pointer so the stack traces in the
  reports are complete.

`out/` is ignored by git, so the binaries never end up in a commit.

## AddressSanitizer: writing past an array

The game stores entities in fixed-size arrays, such as `Coin coins[MAX_COINS]`
inside `GameState`. Before anything is copied, `level_validate_counts()` in
`src/levels/level_validate.c` refuses a level with too many entries.
`labs/debugging/coin_overflow.c` leaves that check out: its "level" has five
coins and its array has room for four.

```sh
./out/labs/coin_overflow
```

```text
==91571==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x6060000001d4 at pc 0x000104928f48 bp 0x00016b4d67e0 sp 0x00016b4d67d8
WRITE of size 4 at 0x6060000001d4 thread T0
    #0 0x000104928f44 in load_coins coin_overflow.c:52
    #1 0x0001049289d8 in main coin_overflow.c:74
    #2 0x000193e8be7c in start+0x1a1c (dyld:arm64e+0x31e7c)

0x6060000001d4 is located 0 bytes after 52-byte region [0x6060000001a0,0x6060000001d4)
allocated by thread T0 here:
    #0 0x0001052f9560 in calloc+0x80 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x41560)
    #1 0x0001049289a8 in main coin_overflow.c:71
    #2 0x000193e8be7c in start+0x1a1c (dyld:arm64e+0x31e7c)

SUMMARY: AddressSanitizer: heap-buffer-overflow coin_overflow.c:52 in load_coins
Shadow bytes around the buggy address:
=>0x606000000180: fa fa fa fa 00 00 00 00 00 00[04]fa fa fa fa fa
...
==91571==ABORTING
```

How to read it, top to bottom:

1. **The kind of bug.** `heap-buffer-overflow` means memory just outside a
   `malloc`/`calloc` block was touched. `WRITE of size 4` says we stored 4 bytes:
   one `float`.
2. **Where it happened.** Frame `#0` is the line that did it:
   `load_coins coin_overflow.c:52`, which is `world->coins[i].x = ...`. Frame `#1`
   is who called it. Read the frames as "this line, called from that line".
3. **Which block it missed.** "0 bytes after 52-byte region" means we wrote
   right past the end. 52 bytes is one `int` (4) plus four 12-byte coins, so
   the fifth coin starts exactly where the block ends.
4. **Where that block came from.** The "allocated by" stack points at the
   `calloc` in `main`. For bigger programs this is often the most useful part.
5. **Shadow bytes.** ASan keeps one shadow byte for every 8 bytes of memory.
   `00` is fully usable, `04` means only the first 4 of those 8 are usable, and
   `fa` is a guard zone it put around the block. The bracket marks our write
   landing in the guard. You rarely need this part; the stacks usually say enough.

ASan stops the program at the first report (exit status 134 here), which is
what we want: the first bad write is the one to fix.

One limit worth knowing. The array is the *last* member of the struct on
purpose. If another field came after it, the fifth coin would quietly
overwrite that field instead: the write stays inside the allocation, so ASan
has nothing to report. That is why the game checks counts *before* copying,
instead of hoping a tool will catch the overflow.

## AddressSanitizer: using a texture after freeing it

`texture_load()` in `src/shared/graphics.h` mallocs a small `Texture2D` handle
and `texture_unload()` frees it. Whoever owns the pointer must clear it after
freeing. `labs/debugging/texture_after_free.c` forgets, then draws one more
frame:

```sh
./out/labs/texture_after_free
```

```text
==91581==ERROR: AddressSanitizer: heap-use-after-free on address 0x602000000954 at pc 0x000100d28ff4 bp 0x00016f0d69e0 sp 0x00016f0d69d8
READ of size 4 at 0x602000000954 thread T0
    #0 0x000100d28ff0 in coin_draw texture_after_free.c:62
    #1 0x000100d28b24 in main texture_after_free.c:84
    #2 0x000193e8be7c in start+0x1a1c (dyld:arm64e+0x31e7c)

0x602000000954 is located 4 bytes inside of 12-byte region [0x602000000950,0x60200000095c)
freed by thread T0 here:
    #0 0x000101535368 in free+0x7c (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x41368)
    #1 0x000100d292c8 in texture_unload texture_after_free.c:56
    #2 0x000100d291e4 in resources_cleanup texture_after_free.c:67
    #3 0x000100d28b14 in main texture_after_free.c:80
    #4 0x000193e8be7c in start+0x1a1c (dyld:arm64e+0x31e7c)

previously allocated by thread T0 here:
    #0 0x000101535274 in malloc+0x78 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x41274)
    #1 0x000100d28c04 in texture_load texture_after_free.c:46
    #2 0x000100d28ad4 in main texture_after_free.c:76
    #3 0x000193e8be7c in start+0x1a1c (dyld:arm64e+0x31e7c)

SUMMARY: AddressSanitizer: heap-use-after-free texture_after_free.c:62 in coin_draw
```

This time there are three stacks, and together they tell the whole story:
the read in `coin_draw` (line 62, `texture->width`), the `free` reached through
`resources_cleanup`, and the original `malloc` in `texture_load`. "4 bytes
inside of 12-byte region" is the `width` field: it sits after the 4-byte `id`.

Now run the plain build, without sanitizers:

```sh
./out/labs/texture_after_free_plain
```

```text
draw coin at x=32 (16x16)
draw coin at x=48 (0x5)
```

No crash, exit status 0, and a nonsense size. The freed memory was reused by
the allocator, so the stale pointer reads whatever is there now. In a bigger
program this is the bug that "only happens sometimes". Without a sanitizer it
can go unnoticed for a long time.

## UndefinedBehaviorSanitizer: signed overflow

In C, adding past `INT_MAX` on an `int` is undefined behaviour: the language
makes no promise about the result, and the compiler may assume it never
happens. `labs/debugging/score_overflow.c` puts a naive score award next to
the saturating one the game uses in `game_award_score()`
(`src/core/game_score.c`):

```sh
./out/labs/score_overflow
```

```text
labs/debugging/score_overflow.c:25:18: runtime error: signed integer overflow: 2147483597 + 100 cannot be represented in type 'int'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior labs/debugging/score_overflow.c:25:18
naive:      -2147483599
saturating: 2147483647
```

A UBSan report is one line: file, line and column, then what went wrong with
the actual values. Column 18 points at the `+` in `score + amount`. The naive
total wrapped round to a large negative number on this machine, but nothing
guarantees even that.

Unlike ASan, UBSan reports and **keeps going** by default, which is why both
lines printed. If you want it to stop at the first problem, as ASan does, add
`-fno-sanitize-recover=undefined` to the compile command.

The fix in the game is to compare before adding,
`amount > INT_MAX - gs->world.score ? INT_MAX : gs->world.score + amount`, and to do the
bonus-life arithmetic in `int64_t`, which has room for any sum of two `int`
values. [C in this codebase](../c-concepts/) lists that and the other
overflow-safe spots.

## A crash in the debugger

`texture_after_free --clear` applies the fix: the owner sets its pointer to
`NULL` after freeing. The late draw is still there, so now it dereferences
`NULL` and the program crashes on that exact line. A loud crash at the right
place is much easier to fix than the quiet garbage we saw above.

Run it under the debugger. On macOS that is `lldb`, which ships with the Xcode
command-line tools:

```sh
lldb out/labs/texture_after_free_plain -- --clear
(lldb) run
```

```text
Process 91920 stopped
* thread #1, queue = 'com.apple.main-thread', stop reason = EXC_BAD_ACCESS (code=1, address=0x4)
    frame #0: 0x00000001000005c4 texture_after_free_plain`coin_draw(texture=0x0000000000000000, x=48) at texture_after_free.c:62:55
   59   static void coin_draw(const FakeTexture *texture, int x)
   60   {
   61       /* Reading texture->width is where a stale pointer is caught. */
-> 62       printf("draw coin at x=%d (%dx%d)\n", x, texture->width, texture->height);
                                                          ^
```

`EXC_BAD_ACCESS` with `address=0x4` is macOS's name for a segmentation fault.
The address is a hint: `0x4` is `NULL` plus the offset of `width`, so we read
a field through a null pointer. The arguments line confirms it:
`texture=0x0000000000000000`.

`bt` (backtrace) prints the call stack, newest call first:

```text
(lldb) bt
* thread #1, queue = 'com.apple.main-thread', stop reason = EXC_BAD_ACCESS (code=1, address=0x4)
  * frame #0: 0x00000001000005c4 texture_after_free_plain`coin_draw(texture=0x0000000000000000, x=48) at texture_after_free.c:62:55
    frame #1: 0x0000000100000518 texture_after_free_plain`main(argc=2, argv=0x000000016fdfefd8) at texture_after_free.c:84:5
    frame #2: 0x0000000193e8be80 dyld`start + 6688
```

The crash is in `coin_draw`, but the mistake is in its caller. `frame select 1`
moves to the caller, and `p` (print) shows a variable there:

```text
(lldb) frame select 1
frame #1: 0x0000000100000518 texture_after_free_plain`main(argc=2, argv=0x000000016fdfefd8) at texture_after_free.c:84:5
-> 84       coin_draw(res.coin, 48);
(lldb) p res
(TextureResources) {
  coin = NULL
}
```

So `main` handed a pointer it had already given up. The fix is not a null
check inside `coin_draw`; it is to stop drawing after cleanup.

On Linux, use `gdb` (install it with your package manager). The same session:

```sh
gdb --args out/labs/texture_after_free_plain --clear
(gdb) run          # stops with "Program received signal SIGSEGV, Segmentation fault."
(gdb) bt
(gdb) frame 1
(gdb) print res
```

| What you want | lldb | gdb |
|---------------|------|-----|
| Start the program | `run` | `run` |
| Stop at a function | `b player_update` | `break player_update` |
| Stop at a line | `b game_score.c:11` | `break game_score.c:11` |
| Show the call stack | `bt` | `bt` |
| Move to a caller | `up`, or `frame select N` | `up`, or `frame N` |
| Print a value | `p gs->world.score` | `print gs->world.score` |
| Locals of this frame | `frame variable` | `info locals` |
| Next line / into a call | `n` / `s` | `next` / `step` |
| Carry on / quit | `c` / `q` | `continue` / `quit` |

## Stepping through the game

The same commands work on the real game. `make debug` builds the game and
editor with `-g -O0` into `out/debug/`. Start the debugger from the
repository root so the asset paths resolve, and give the game a lab level:

```sh
make debug CC=clang
lldb out/debug/super-mango -- --no-save --level levels/labs/01_collision.toml
(lldb) b level_load
(lldb) b player_update
(lldb) run
```

The game stops first in `level_load()`, before the first frame is drawn:

```text
* thread #1, queue = 'com.apple.main-thread', stop reason = breakpoint 1.1
    frame #0: 0x000000010001bea4 super-mango`level_load(gs=0x00000074bbdb0000, def=0x00000074bc140000) at level_loader.c:575:32
-> 575      if (level_validate_runtime(def, err, sizeof(err)) != 0) {
(lldb) bt
  * frame #0: ... super-mango`level_load(gs=0x00000074bbdb0000, def=0x00000074bc140000) at level_loader.c:575:32
    frame #1: ... super-mango`game_level_load_initial(gs=0x00000074bbdb0000) at level_session.c:411:9
    frame #2: ... super-mango`game_init(gs=0x00000074bbdb0000) at game_lifecycle.c:50:9
    frame #3: ... super-mango`session_make_game(session=0x00000074bc400000, path="levels/labs/01_collision.toml", inherited=0x0000000000000000) at app_session.c:217:9
    frame #4: ... super-mango`session_open_game(session=0x00000074bc400000, path="levels/labs/01_collision.toml", inherited=0x0000000000000000) at app_session.c:240:21
    frame #5: ... super-mango`session_create(config=0x000000016fdfe8a0) at app_session.c:455:13
    frame #6: ... super-mango`main(argc=6, argv=0x000000016fdfef98) at main.c:121:27
(lldb) p def->coin_count
(const int) 1
```

Read that backtrace from the bottom up and you have the startup path:
`main` creates a session, the session makes a game, `game_init()` loads the
first level, and `level_load()` validates it before copying anything. The
collision lab has one coin.

`c` (continue) runs on to the first simulation step:

```text
(lldb) c
* thread #1, queue = 'com.apple.main-thread', stop reason = breakpoint 2.1
    frame #0: ... super-mango`player_update(player=0x00000074bbdb0230, dt=0.0166666675, ..., world_w=800) at player.c:58:32
(lldb) bt
  * frame #0: ... super-mango`player_update(player=0x00000074bbdb0230, dt=0.0166666675, ...) at player.c:58:32
    frame #1: ... super-mango`game_player_step(gs=0x00000074bbdb0000, dt=0.0166666675) at game_player_step.c:30:5
    frame #2: ... super-mango`game_update_active(gs=0x00000074bbdb0000, dt=0.0166666675, cam_x=0) at game_update.c:28:21
    frame #3: ... super-mango`game_frame(gs=0x00000074bbdb0000) at game_loop.c:59:17
    frame #4: ... super-mango`session_step(session=0x00000074bc400000, callback_owned=0) at app_session.c:483:13
    frame #5: ... super-mango`session_run(session=0x00000074bc400000) at app_session.c:535:9
    frame #6: ... super-mango`main(argc=6, argv=0x000000016fdfef98) at main.c:123:18
(lldb) frame select 1
(lldb) p gs->world.player
(Player) {
  x = 48
  y = 220
  vx = 0
  vy = 0
  speed = 160
  w = 48
  h = 48
  on_ground = 1
  anim_state = ANIM_IDLE
  ...
}
```

Things to notice:

- `dt=0.0166666675` is 1/60 s as a `float`: the fixed simulation step.
  It is the same on every call, whatever your monitor's refresh rate.
- `player_update()` only receives a `Player *`, not the whole `GameState`.
  To see `gs`, step up one frame to `game_player_step()`, which has it.
- `p gs->world.player` prints the struct by value because `Player` is stored by
  value inside `GameState`. lldb shortens long structs with `...`; print one
  field with `p gs->world.player.vx`, or every field with `frame variable -A gs->world.player`.
- `player=0x...0230` is `gs` plus a small offset: the player lives inside the
  game state allocation, not in a separate one.

Try this: delete the `level_load` breakpoint (`br del 1`) and make the
`player_update` one conditional, so it only stops once Mango moves:

```sh
(lldb) br del 1
(lldb) breakpoint modify --condition 'player->vx > 0' 2
(lldb) c
```

Walk right in the game window. The debugger stops on the first step with a
positive `vx`; `p player->vx` after each `c` shows it growing by the walk
acceleration until it reaches `walk_max_speed`. In gdb the same condition is
`condition 2 player->vx > 0`.

For the editor, the same approach works on `out/debug/super-mango-editor`; good
first breakpoints are `undo_push` and `editor_commit_change`.

## Sanitizers on the whole game

`make sanitize CC=clang` builds the game, editor and tests with
`-fsanitize=address,undefined` into a separate `out-sanitize/` tree, runs the
test suite, then replays the fuzz corpus. `make sanitize-smoke CC=clang` boots
every level under the sanitizers as well. When one of them prints a report,
read it exactly as above: the kind of bug, the first frame in our code, then
the allocation and free stacks. The [Testing](../testing/) page says when to
run which.

To play a level by hand under the sanitizers, run the instrumented binary
directly once `make sanitize` has built it:

```sh
./out-sanitize/super-mango --debug --no-save --level levels/labs/05_hazards.toml
```

## A quick profiling pass

A profiler answers "where does the time go?" by stopping the program
thousands of times a second and noting which function it was in. Profile an
optimised build: at `-O0` the numbers mostly measure missing optimisations.
`make release` uses `-O2`; adding `EXTRA_CFLAGS=-g` keeps the function and
line names readable.

```sh
make release CC=clang EXTRA_CFLAGS=-g
./out/release/super-mango --no-save --level levels/02_lugio_02.toml --smoke-test-frames 30000 &
sample super-mango 5 -file out/labs/sample.txt     # macOS: 5 seconds of samples
```

`levels/02_lugio_02.toml` is the biggest campaign stage, and
`--smoke-test-frames` makes the game run a fixed number of frames and quit by
itself. The top of the main thread's call graph, trimmed:

```text
Call graph:
    2161 Thread_7924528: Main Thread   DispatchQueue_<multiple>
    + 2161 start  (in dyld) + 6688  [0x193e8be80]
    +   2161 main  (in super-mango) + 884  [0x102f3130c]  main.c:123
    +     2161 session_run  (in super-mango) + 104  [0x102f33a90]  app_session.c:535
    +       2155 session_step  (in super-mango) + 280  [0x102f33364]  app_session.c:483
    +       ! 2141 game_frame  (in super-mango) + 192  [0x102f38120]  game_loop.c:67
    +       ! : 1967 game_render_frame  (in super-mango) + 2248  [0x102f4ae64]  game_render.c:357
    +       ! : | 1734 display_present  (in super-mango) + 228  [0x102f505f4]  graphics.c:41
    ...
    +       ! 14 game_frame  (in super-mango) + 152  [0x102f380f8]  game_loop.c:59
    +       !   8 game_update_active  (in super-mango) + 88  [0x102f3b038]  game_update.c:28
```

Each number is how many samples landed in that function or anything it
called. Of 2161 samples, 1734 were in `display_present()` handing the
finished frame to the GPU and waiting for it, and only 14 were in the
simulation (`game_update_active()` through `game_loop.c:59`). So on this level
the C game logic is cheap; the time goes to drawing and presenting. If a
change makes the game slow, measure first: a profile like this tells you
whether to look at `src/render/` or at the update code before you change
anything.

For a graphical view on macOS, open the same run in Instruments' Time Profiler:
`xcrun xctrace record --template 'Time Profiler' --launch -- ./out/release/super-mango --no-save --level levels/02_lugio_02.toml --smoke-test-frames 30000`,
then open the `.trace` file it writes.

On Linux, `perf` does the same job:

```sh
perf record -g ./out/release/super-mango --no-save --level levels/02_lugio_02.toml --smoke-test-frames 30000
perf report            # interactive; sorted by where the samples landed
```

`make timing-lab` is a different kind of measurement worth knowing: it shows
why the game advances in fixed 1/60 s steps. [Sandbox School](../learning-path/)
lab 2 uses it.
