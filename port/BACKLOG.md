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
