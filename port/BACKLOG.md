# PC port backlog

Tickets for the native PC port on `pc-port`. The melee-port agent works
them unattended every 2 hours (an OpenClaw automation), one ticket per run,
following the procedure below. Chuck edits this file to add, reorder or
drop tickets; the top-most `open` ticket is worked first.

Roadmap context: `AGENTS.md` (not in git) and the step list in the repo's
agent notes. Step 2 (real data, simulation) is in progress; step 3 (window,
input) needs a Windows build first.

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

### PORT-001: Swap itPublicData (ItCo.usd)
- Status: done
- Why: ItCo.usd `itPublicData` is loaded every match and entirely
  big-endian (~14k unconverted objects in `MELEE_PORT_SWAP_AUDIT`). Items
  are off in the test match, so nothing crashes yet, but item spawns and
  common articles read it.
- Do: a typed walker in `port/src/swap_item.c` (decomp `it/types.h`,
  HSDLib `itPublicData.cs`): common item articles (reuse
  `port_swap_article`), item state tables and scripts, and whatever else
  hangs off the root.
- Done when: the "no type known ... itPublicData" warning is gone;
  `MELEE_PORT_SWAP_AUDIT` shows no float/int-looking unconverted objects
  in ItCo.usd (strings, keyframes and GPU payloads are fine); check.sh
  passes.
- Log:
- 2026-09-16 16:47: done. `port_swap_itPublicData` (swap_item.c): generated
  `ItemCommonData` swap; the 43 common, 118 from-Kuriboh and 49 Pokemon
  article tables (sizes from Item_80267978's kind ranges, NULL slots
  skipped); it_804D6D40_t words; color-anim rows. Article fixes: dynamics
  +0xC hit bubbles, 0x18-byte model descs (+0x14 f32[8]). Pointers in
  special attributes handled per kind: Foods (28 joints), Kinoko/DKinoko
  and WStar (anim joints), Kuriboh/Leadead/Octarock/Ottosea (+0 word
  block), Unk4 (3x joint/anim/matanim rows), Unknown + Unknown_Swarm (26
  joints). Audit: "no type known ... itPublicData" gone; flagged ItCo.usd
  objects 7396 -> 4762, all keyframes (3235 FObjDesc `ad`), display lists,
  vertex/texture/palette data, plus 4 model groups (0x25f0bc, 0x66cc8,
  0x74af4, 0xaa4a0) that no relocation points to. Trace changed from f180:
  ftcoll.c reads ItemCommonData xD4/x78 on fighter hits, previously BE
  garbage (bisected: only the ItemCommonData swap changes it); baseline
  updated. check.sh: ALL GATES PASSED, 34 "no type known" (was 36).

### PORT-002: Items-on forced match
- Status: open
- Depends: PORT-001
- Do: `MELEE_PORT_DEMO_ITEMS=1` keeps items on (default frequency) in the
  forced demo match; fix what breaks (item spawn, pickup, throw).
- Done when: Fox vs Marth on the line stage with items on reaches 9000
  retraces with exit 0, and the trace shows at least one item spawned
  (add item spawns to `MELEE_PORT_TRACE` output).
- Log:

### PORT-003: Battlefield from the real DAT
- Status: open
- Do: `MELEE_PORT_DEMO_MATCH=1` without `MELEE_PORT_STAGE=line` uses
  Battlefield (`GrNBa.dat`). Complete the stage swaps (map_head models,
  yakumono, particles, `ALDYakuAll`, `itemdata`) and stage callbacks until
  it runs. Stage-only items whose special attributes have s16/u8 fields
  need typed swaps like Redead/Octorok got in PORT-005 (add them to
  `gen_swap.py` TYPES and swap them before the extent pass):
  `itLikelikeAttributes` (u8 +3C..+3E), `itTincleAttributes` (u8 +54/+55),
  `itWhiteBeaAttributes` (s16 +08..+14), `itOldottoseaAttributes`
  (u8 +10, +28).
- Done when: the Battlefield forced match reaches 9000 retraces with exit 0;
  fighters stand on the platforms (trace y > 0 on ground at some point).
  Add it as a second gate in check.sh with its own baseline trace.
- Log:

### PORT-004: Remaining untyped public symbols in a match
- Status: open
- Do: add walkers for the symbols check.sh still reports as "no type
  known": `SIS_IntroData`, `SIS_MessageData` (sislib text),
  `lbRumbleData`, `ty*Tbl` (trophy tables), `TitleMark_sobjdesc`,
  `MemCardIconData`, `lbBgFlashColAnimData`. One commit per symbol family.
- Done when: check.sh reports 0 "no type known" warnings.
- Log:

### PORT-006: Forced match for any two characters
- Status: open
- Do: `MELEE_PORT_DEMO_MATCH=<ckind>,<ckind>` picks the characters (keep
  `1` = Fox vs Marth). Then run every character against Fox on the line
  stage for 3000 retraces after match start and fix crashes. Split into
  one ticket per character or group if needed.
- Done when: all 25 selectable characters (plus Zelda/Sheik transform and
  Ice Climbers pair) survive the run; results table logged here.
- Log:

### PORT-010: Source-line map of MWCC fused multiply-adds
- Status: open
- Why: see `port/docs/fp-parity.md`. 3683 fused instructions in 819
  retail functions; clang's own contraction agrees in only 379 functions,
  so each site must be made explicit.
- Do: build the matching objects with `-sym on` into a scratch build
  directory (don't touch `build/`), decode fused instructions (opcode 59/63,
  XO 28..31) per object, map them to file:line through the DWARF 1 `.line`
  tables, and write `port/docs/fma-sites.txt` (file, line, function, kind;
  no game data). Also commit the scanner as `port/tools/fma_scan.py`, which
  can count fused instructions per function in both the DOL and
  `port/build/melee`.
- Done when: the site list covers all 3683 instructions (or explains the
  remainder) and `fma_scan.py` reproduces the counts in the doc.
- Log:

### PORT-011: Explicit fused math in gameplay code
- Status: open
- Depends: PORT-010
- Do: add `PORT_FMADD`/`PORT_FMSUB`/`PORT_FNMSUB`/`PORT_FNMADD` macros
  (plain expressions for MWCC, `fmaf`-based under MELEE_PORT) and apply
  them at the listed sites in `lb/lbcollision.c`, `lb/lbvector.c`,
  `mp/`, `cm/camera.c`, then `ft/` and `it/`, one directory per commit.
  Extend `check.sh` with a gate: for converted directories, per-function
  fused counts in the port equal the retail counts.
- Done when: gameplay directories are converted and gated; the trace
  change is explained in the PR.
- Log:

### PORT-012: Run melee.exe on Windows
- Status: needs-chuck
- Do: follow `port/docs/windows.md` on a 64-bit Windows PC: run the
  headless test match and compare the trace with
  `port/tests/demo_line.trace`. Paste the log tail (and the trace diff if
  any) into this ticket's log, or tell a chat session.
- Log:

### PORT-009: Dolphin comparison tooling
- Status: needs-chuck
- Do: define a per-frame state dump (position, velocity, action state,
  percent, frame counters, RNG seed) that can be produced both by the port
  and by Dolphin (memory watch / Lua script at known addresses), plus a
  diff tool. Needs Chuck to run Dolphin and record the same match.
- Log:
