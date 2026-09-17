# PC port backlog

Tickets for the native PC port on `pc-port`. The melee-port agent works
them unattended every 2 hours (an OpenClaw automation), one ticket per run,
following the procedure below. Chuck edits this file to add, reorder or
drop tickets; the top-most `open` ticket is worked first.

Roadmap context: `AGENTS.md` (not in git) and the step list in the repo's
agent notes. Step 2 (real data, simulation) is in progress; step 3 (window,
input, debug draws) is ticketed as PORT-019..028 and runs after the step 2
tickets. A ticket's `Depends:` tickets must be merged first.

## Procedure (for the automated run)

1. **Worktree.** Work only in `.worktrees/bot` (a git worktree of this repo;
   `.venv` and `orig` are symlinks to the main checkout). Never edit the
   main checkout, which Chuck's chat sessions use. If a lock file
   `.worktrees/bot.lock` is younger than 3 hours, stop: another run is
   active. Otherwise write the lock (date + ticket id) and remove it when
   done.
2. **Sync.** `git fetch origin`. If a `ticket/*` branch for an
   `in-progress` ticket exists, continue it (rebase on `origin/pc-port`);
   else check out a new `ticket/<id>-<slug>` from `origin/pc-port`, and
   pick the first `open` ticket whose dependencies are merged (no longer
   listed). Skip any ticket with an `Owner:` line other than `bot`: a chat
   session has claimed it.
3. **Work** to the acceptance criteria. Follow `AGENTS.md` rules (no game
   data in git, `#ifdef MELEE_PORT` in shared code, matching build
   green). Commit small logical steps on the ticket branch and push it, so a
   run that times out loses nothing. Stop starting new work after ~45
   minutes; log where you are.
4. **Verify** with `port/tools/check.sh` (all gates must pass). If the
   trace changed on purpose, run `port/tools/check.sh --update-trace`,
   commit the new `port/tests/demo_line.trace`, and explain the change in
   the PR body.
5. **Land.** Remove the ticket from this file in the same branch (the PR
   body is the record: what changed, check.sh output, trace changes), then:
   `gh pr create --repo chotaling1/melee --base pc-port --head <branch>`
   with a body listing what changed and the check.sh output, and
   `gh pr merge --repo chotaling1/melee --merge --delete-branch <pr>`.
   **Always pass `--repo chotaling1/melee`**; never target doldecomp/melee.
   GitHub Actions minutes are exhausted, so do not wait for or rely on CI
   checks; `check.sh` is the gate.
6. **Blocked or out of time.** Keep the branch; set the ticket to
   `in-progress` (resumable) or `blocked` with the reason, and append a log
   entry. Push ticket-status-only edits of this file straight to `pc-port`
   (no PR) so the next run sees them. Tickets marked `needs-chuck` are
   skipped.
7. **New tickets.** Splits and direct prerequisites of an existing ticket
   may be added as `open` right above it. New scope goes to the bottom as
   `proposed` and is not worked until Chuck changes it to `open`.
8. **Log format:** under the ticket, `- YYYY-MM-DD HH:MM: <what happened,
   PR/commit, check.sh result, next step>`. Keep entries short and
   factual; include real numbers and failures.

Statuses: `open`, `in-progress`, `blocked`, `needs-chuck`, `proposed`.
Merged tickets are deleted from this file. Chat sessions claim a ticket by
adding `- Owner: chat` (pushed to `pc-port` before starting) and work in
their own worktree.

## Tickets

### PORT-013: Samus: texture animation crash
- Status: open
- Do: `MELEE_PORT_DEMO_MATCH=16,2 MELEE_PORT_STAGE=line` crashes in
  `HSD_TObjAddAnim` (tobj.c) <- `HSD_MObjAddAnim` <- `HSD_JObjAddAnim`
  (addr 0x3000300: unconverted texture/material animation data, likely an
  article or effect model's matanim). Find the unconverted object with
  `MELEE_PORT_SWAP_AUDIT=1` and fix its walker.
- Done when: `port/tools/char_matrix.sh` shows ckind 16 exit 0.
- Log:

### PORT-014: Link and Young Link: hookshot and boomerang
- Status: open
- Do: Link (`6,2`) asserts `jobj->child` in `HSD_JObjResolveRefs` from
  `it_802A2568` (itlinkhookshot.c); Young Link (`21,2`) asserts `jobj` in
  `it_80273B50` (it_2725.c) from `it_802A0534` (itlinkboomerang.c). Both
  load article models from the fighter's `x48_items`; check the article
  model/state data (and the non-article slots, see PORT-017).
- Done when: ckind 6 and 21 exit 0 in `char_matrix.sh`.
- Log:

### PORT-015: Ness and Mr. Game & Watch
- Status: open
- Do: Ness (`11,2`) panics in dobj.c:312 while loading a model (recursive
  `DObjLoad`: a DObj `next` chain loops or points at unconverted data);
  Game & Watch (`3,2`) segfaults in `HSD_DObjSetFlags` from
  `ftParts_800750C8` (model part visibility tables, ftData +08).
- Done when: ckind 11 and 3 exit 0 in `char_matrix.sh`.
- Log:

### PORT-016: Kirby hangs
- Status: open
- Do: Kirby (`4,2`) never returns (timeout) right after stage init, i.e.
  during fighter creation. Run with `MELEE_PORT_ABORT_AT_LIMIT=1` and a
  low `MELEE_PORT_FRAMES` or attach a watchdog to find the loop; Kirby
  loads extra data (copy abilities, per-character hat data in each
  fighter's `x48_items`).
- Done when: ckind 4 exits 0 in `char_matrix.sh`.
- Log:

### PORT-017: Non-article entries in ftData x48_items
- Status: open
- Why: `x48_items` mixes Article* with other data (Fox [4]: s32 table;
  Samus [4]: { HSD_Joint*; AnimJoint**; MatAnimJoint*; ... } used for
  Kirby's copied hat). Since PORT-006 those get only the word pass, so
  their models/animations stay big-endian.
- Do: identify each character's extra slots (decomp: ftkirbyspecials.c
  `x48_items[4]`, ftyoshispecialn.c `[3]`, HSDLib) and walk them properly.
- Done when: `MELEE_PORT_SWAP_AUDIT` shows no unconverted models/anims
  reachable from x48_items for all characters.
- Log:

### PORT-011: Explicit fused math in gameplay code
- Status: open
- Do: add `PORT_FMADD`/`PORT_FMSUB`/`PORT_FNMSUB`/`PORT_FNMADD` macros
  (plain expressions for MWCC, `fmaf`-based under MELEE_PORT) and apply
  them at the sites in `port/docs/fma-sites.txt` in `lb/lbcollision.c`, `lb/lbvector.c`,
  `mp/`, `cm/camera.c`, then `ft/` and `it/`, one directory per commit.
  Extend `check.sh` with a gate: for converted directories, per-function
  fused counts in the port (`fma_scan.py port`, built with `-mfma`) equal
  the retail counts (`fma_scan.py dol`).
- Done when: gameplay directories are converted and gated; the trace
  change is explained in the PR.
- Log:

<!-- Roadmap step 3: window, input, debug draws. Scoped 2026-09-16 with
Chuck. Design: the debug renderer rasterizes into a CPU framebuffer
(port-owned, no GPU), so Linux can dump frames to image files for check.sh
and Windows shows the same buffer in an SDL window. Only the GX calls the
game's own debug draws use get implemented; everything else stays a stub
(step 4 replaces this with a real GX backend). Gameplay must not change:
the fighter trace with drawing on must equal the headless trace. -->

### PORT-019: Debug framebuffer and frame dumps (no window)
- Status: open
- Do: add `port/src/fb_port.c` + `port/include/port/fb.h`: a 640x480 RGBA
  framebuffer with a depth buffer, clear, clipped line (width in pixels) and
  flat/vertex-colored triangle rasterization with depth test, alpha blend.
  Once per retrace (vi_port.c, after the game's draw pass) the frame is
  "presented": with `MELEE_PORT_DUMP_FRAMES=a-b[/step]` it writes
  `frame_NNNNN.bmp` (BMP writer, no deps) to `MELEE_PORT_DUMP_DIR` (default
  cwd). Nothing draws into it yet except a test pattern behind
  `MELEE_PORT_FB_TEST=1`.
- Done when: a Linux run with FB_TEST dumps the expected pattern (check a
  few pixel values in a small `port/tools/bmp_check.py`); the default
  headless run and trace are unchanged; check.sh passes.
- Log:

### PORT-020: GX debug-draw subset into the framebuffer
- Status: open
- Depends: PORT-019
- Do: `port/src/gx_debug.c` implements, on the CPU, the GX calls used by
  `lb/lbcollision.c` and `mp/mplib.c` debug draws: GXSetProjection (state
  exists in gx_port.c), GXSetViewport, GXLoadPosMtxImm/GXSetCurrentMtx,
  GXClearVtxDesc/GXSetVtxDesc/GXSetVtxAttrFmt, GXBegin/GXPosition3f32/
  GXColor4u8/GXEnd (GX_POINTS, LINES, LINESTRIP, TRIANGLES, TRIANGLESTRIP,
  TRIANGLEFAN, QUADS), GXSetLineWidth/GXSetPointSize, GXSetZMode,
  GXSetCullMode, GXSETARRAY/GXSetArray + GXCallDisplayList for the
  executable's static index display lists (`lbColl_SphereDisplayList`,
  `lbColl_CylinderDisplayList`; big-endian command bytes). Material/TEV
  state is ignored (vertex or channel color only).
  Only emit geometry inside an explicit debug scope
  (`port_gx_debug_begin/end`), so model display lists, shadows, bg flash
  and afterimages (which also call GXBegin/GXCallDisplayList) draw nothing.
  Verify the GX weak stubs these replace are removed/overridden.
- Done when: a port test hook draws one `lbColl_` sphere at a known world
  position with the match camera and the dumped frame shows it where
  `GXProject` puts that position (pixel check in bmp_check.py).
- Log:

### PORT-021: Stage collision, blast zones and camera bounds
- Status: open
- Depends: PORT-020
- Do: `MELEE_PORT_DEBUG_DRAW` (comma list; this ticket: `coll`) runs a port
  debug pass after the game's camera render (see camera.c ~4030, which
  already calls mpLib_8005A2DC/mpLib_DrawZones/mpLib_DrawSpecialPoints
  when its debug flags are set). Use the game's collision line draw
  (mpLib_DrawMatchingLines colors per line kind: floor, ceiling, walls,
  ledges, platforms) inside the debug scope, plus port-drawn rectangles for
  blast zones and camera limits (stage general points 0x95-0x98).
- Done when: on the line stage (`MELEE_PORT_STAGE=line`,
  `MELEE_PORT_DEMO_MATCH=1`) a dumped frame shows the floor from -85.57 to
  85.57 and the blast zone box; trace identical to headless; add a
  check.sh gate dumping one frame and checking it with bmp_check.py.
- Log:

### PORT-022: Hitboxes, hurtboxes and ECBs
- Status: open
- Depends: PORT-021
- Do: `MELEE_PORT_DEBUG_DRAW=hitbox` sets the game's own per-fighter
  display mode (`Fighter.x21FC_flag`, semantics in db/dbanim.c) so
  ftDrawCommon_800805C8 / itdraw.c draw hit capsules, hurt capsules,
  shields, reflect/absorb bubbles in the game's colors, inside the debug
  scope. `ecb` draws mpLib_DrawEcbs. If a mode bit also hides the model or
  changes logic, set only what drawing needs and note it in the PR.
- Done when: a dumped frame during a Fox attack (pick the frame from
  `MELEE_PORT_TRACE=1`) shows a hitbox; hurtboxes visible on both
  fighters; trace identical to headless; check.sh gate extended.
- Log:

### PORT-023: Skeletons
- Status: open
- Depends: PORT-020
- Do: `MELEE_PORT_DEBUG_DRAW=skel` draws each fighter's (and held item's)
  JObj tree as parent-to-child lines from world matrices, one color per
  player, joints as points. Headless runs may skip matrix setup, so call
  HSD_JObjSetupMatrix (or equivalent) from the port pass only; it must not
  change the trace.
- Done when: a sequence of dumped frames shows the stick figure following
  the trace motion (e.g. a jump: y rises then falls); trace identical.
- Log:

### PORT-024: SDL3 for the Windows build
- Status: open
- Do: add SDL3 (pinned release, e.g. 3.2.x) for `x86-windows-gnu` only.
  First try the official `SDL3-devel-<ver>-mingw` package (i686 import lib
  + SDL3.dll): fetch it in a script (`port/tools/fetch_sdl.py`, checksum
  pinned, extracted under `port/third_party/`, gitignored) and link with
  zig. If zig cannot link it, build SDL3 from source with zig cc using
  SDL's `SDL_build_config_windows.h`. Document in port/docs/windows.md.
  The Linux build stays SDL-free.
- Done when: melee.exe links with SDL; `SDL3.dll` is placed next to the
  exe by the build; check.sh still passes (Windows link step).
- Log:

### PORT-025: Window and real-time pacing (Windows)
- Status: open
- Depends: PORT-019, PORT-024
- Do: `MELEE_PORT_WINDOW=1` opens a resizable window (640x480 logical,
  4:3 letterboxed), uploads the framebuffer to an SDL texture each
  retrace, pumps events, paces retraces to 59.94 Hz wall clock (the time
  model stays retrace-driven), and exits 0 when the window closes. Esc
  quits. Without the variable nothing changes.
- Verify on the Windows node (see port/docs/windows.md: run via
  `exec host=node` PowerShell `-EncodedCommand`, the WSL share is at
  `\\wsl.localhost\OpenClawGateway\...`). If a node-launched process
  cannot show a window on the desktop, verify with MELEE_PORT_DUMP_FRAMES
  from the windowed build instead and set PORT-027 to cover the visual
  check.
- Done when: windowed run on the node reaches the match and the dumped (or
  captured) frames show the PORT-021/022 debug draws; 9000-retrace run
  with window matches the headless trace; average frame time 16.68 ms
  +/- 0.2 logged at exit.
- Log:

### PORT-026: Controller and keyboard input
- Status: open
- Depends: PORT-025
- Do: replace the PADRead path in pad_port.c (keep MELEE_PORT_INPUT for
  tests; it wins when set) with SDL gamepads for ports 1-4: main stick and
  C-stick to s8, analog triggers to u8 (plus digital L/R at full press),
  A B X Y Z Start and D-pad. Keyboard fallback for port 1 (document the
  layout). Hot-plug. Use SDL's GameCube adapter support where available and
  document the WinUSB (Zadig) requirement. Sample once per retrace, before
  the game polls. Test with SDL's virtual joystick
  (SDL_AttachVirtualJoystick) driven by a script so the bot can verify.
- Done when: a virtual-joystick run moves port 1 in a way the trace shows
  (e.g. hold right: Fox dashes, pos x increases); docs list the mappings.
- Log:

### PORT-027: Human-controlled test match
- Status: open
- Depends: PORT-026
- Why: MELEE_PORT_DEMO_MATCH hijacks the attract demo (both CPU, and a
  button press would normally end it).
- Do: `MELEE_PORT_MATCH=<p1ckind>,<p2ckind>[,stage]` starts a real VS match
  directly (skip menus): port 1 human, port 2 CPU (`MELEE_PORT_CPU_LEVEL`,
  default 9), 4 stocks, timer off. Pause with Start works as in game.
  Debug hotkeys: F1-F4 toggle coll/hitbox/ecb/skel, F5 frame advance while
  paused.
- Done when: scripted/virtual input controls Fox in the trace; CPU acts;
  a match ends on stocks with exit 0 back to a result or restart.
- Log:

### PORT-009: Dolphin comparison tooling
- Status: needs-chuck
- Do: define a per-frame state dump (position, velocity, action state,
  percent, frame counters, RNG seed) that can be produced both by the port
  and by Dolphin (memory watch / Lua script at known addresses), plus a
  diff tool. Needs Chuck to run Dolphin and record the same match.
- Log:

### PORT-028: Chuck plays it
- Status: needs-chuck
- Depends: PORT-027
- Do: on Windows, run the documented windowed match with a controller
  (and the GameCube adapter if available); report input feel/latency,
  window behavior, anything wrong in the debug draws. Findings become
  tickets.
- Log:

### PORT-029: Battlefield diverges between Linux and Windows
- Status: proposed
- Why: found while landing PORT-003. The line-stage trace is still
  byte-identical across the two builds, but the Battlefield forced match
  (`MELEE_PORT_DEMO_MATCH=1`, no `MELEE_PORT_STAGE`) differs: at f1200 of
  the first demo Fox has 31.0% on Windows and 32.0% on Linux while
  standing still on the left platform (-45.061, 27.200); the second demo
  then diverges completely (116 diff lines out of 102 trace lines).
  A 1% difference on a motionless fighter is the magnifier tick
  (`Fighter_8006A360`), the same symptom PR #9 traced to an off-screen
  test fed by uninitialized data - so suspect another weak GX/VI stub with
  an out-pointer, or state the Battlefield camera path reads and the line
  stage never touches. Both builds are `-msse2 -mfpmath=sse
  -ffp-contract=off`, so plain codegen FP differences are unlikely.
- Do: bisect with `MELEE_PORT_TRACE=1` around f1150-1250 on both builds,
  find the first differing value, and audit the stubs/state it comes from
  (`port/src/sdk_stubs_gen.c` out-pointers first). Then make the
  Battlefield gate cross-platform in port/docs/windows.md.
- Done when: the Windows Battlefield trace equals
  `port/tests/demo_bf.trace`.
- Log:
  - 2026-09-16 22:35: filed from the PORT-003 run. Windows logs were
    captured with `cmd /c "melee.exe 2> log"`; PowerShell's own `2>`
    redirect wraps native stderr at the console width and loses line
    tails.
