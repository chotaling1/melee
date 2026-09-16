# Floating-point parity: fused multiply-add (PORT-007 findings)

Measured 2026-09-16 on the retail NTSC 1.02 `main.dol` and the port build at
`f90df680e`.

## Why it matters

Game code is compiled with MWCC `-fp_contract on` (`configure.py`,
`cflags_base`); the Dolphin SDK libraries use `-fp_contract off`. With
contraction on, MWCC emits PowerPC fused instructions (`fmadds`, `fmsubs`,
`fnmsubs`, `fnmadds`) for `a * b + c` style expressions. A fused operation
rounds once; the unfused `a * b` then `+ c` rounds twice. Results differ in
the last bits, which is enough to diverge physics, collision and DI over a
few hundred frames. The port currently builds with `-ffp-contract=off`
(never fused), so every fused site in the retail code is computed
differently on PC.

## Retail counts

Decoded from the DOL text sections (opcode 59 = single, 63 = double; A-form
XO 28..31), mapped to functions with `config/GALE01/symbols.txt` and to
files with `splits.txt`:

| Instruction | Count |
|---|---|
| fmadds | 2204 |
| fnmsubs (double) | 860 |
| fnmsubs | 258 |
| fmsubs | 215 |
| fmadds (double) | 119 |
| fmsubs (double) | 14 |
| fnmadds | 13 |
| **total** | **3683 in 819 functions** |

By directory (instructions / functions): `melee/ft` 834 / 287,
`sysdolphin/baselib` 793 / 59, `melee/it` 663 / 177, `melee/lb` 432 / 43,
`melee/gr` 351 / 102, `melee/gm` 130 / 39, `melee/mp` 112 / 25, `melee/cm`
98 / 23, `melee/mn` 75 / 30, `melee/ty` 63 / 11, `dolphin/axfx` 50 / 3,
others < 40.

Top files: `it/kinds/itlinkhookshot.c` 195, `ft/kinds/ftCommon/ftCo_0A01.c`
186, `sysdolphin/baselib/psdisp.c` 184, `lb/lbcollision.c` 156,
`sysdolphin/baselib/mtx.c` 131, `lb/lbvector.c` 118, `cm/camera.c` 98,
`ft/ftcpuattack.c` 96, `it/kinds/itsamusgrapple.c` 86, `gr/grzebes.c` 85,
`mp/mplib.c` 69, `sysdolphin/baselib/spline.c` 64.

Gameplay-relevant subset (names matching ft/mp/lbColl/lbVector/lb_/Camera/
it_ prefixes): 458 functions, 1887 fused instructions. Hitbox collision
(`lbcollision.c`), vector math (`lbvector.c`), environment collision
(`mplib.c`, `mpcoll.c`) and fighter physics (`ft/`) are all in it.

## Does clang's own contraction reproduce it? No.

The port was rebuilt with `-ffp-contract=fast -mfma` (build dir
`port/build-fma`, not committed) and every function's `vfmadd*/vfmsub*/
vfnmadd*/vfnmsub*` instructions were counted with capstone:

- clang fuses 2870 instructions in 812 functions
- 667 functions fuse in both; only **379** have the same count
- 152 functions fuse only under MWCC (e.g. `grZebes_801DBB60` 34,
  `drawShapeAnim` 30, `sinf`/`cosf` 13, `ftCo_800A9CB4` 9,
  `resolveIKJoint2` 9); 145 only under clang (`PSMTXConcat` 24,
  `HSD_JObjMakePositionMtx` 17, `C_MTXLookAt` 13, `ftCo_800ABA34` 13)
- gameplay subset: 204 of 458 functions agree

Counts are an upper bound on agreement (equal counts can still be different
expressions), and inlining at -O1 moves some sites. A global compiler flag
is not a fix.

## Proposed approach

1. **Locate every fused site at source-line level.** Rebuild the matching
   objects with `-sym on` into a scratch build dir (MWCC then emits DWARF 1
   `.line` tables), decode the fused instructions per object, and map them
   to file:line. Output a checked-in list `port/docs/fma-sites.txt` (no game
   data: file, line, instruction kind).
2. **Make those expressions fused on PC explicitly.** Keep
   `-ffp-contract=off` globally and rewrite each listed expression under
   `#ifdef MELEE_PORT` with a macro such as `PORT_FMADD(a, b, c)` expanding
   to `fmadds`-equivalent `fmaf(a, b, c)` (and `-mfma` so it's a single
   instruction, or musl's exact software `fmaf`, which gives identical
   results). Gameplay directories first (`ft`, `it`, `lb`, `mp`, `cm`,
   `gr`), then the rest.
3. **Verify mechanically.** A tool compares, per function, the MWCC fused
   instruction count with the port's fused count (the scan used here);
   gate it in `check.sh` for the directories already converted.

Other precision notes seen while measuring: 993 double-precision fused
instructions (opcode 63) sit mostly in `mtx.c`, `psdisp.c` and the AX reverb
code, i.e. double math in rendering/audio, not gameplay. x86 SSE scalar
single precision already rounds per operation like PPC single ops, so the
remaining known gap is contraction.

## Site map (PORT-010)

`port/tools/fma_scan.py` reproduces the counts above (`dol` mode) and maps
every fused instruction to a source line (`objs` mode) using MWCC objects
built with `-sym on`, whose DWARF 1 `.line` tables give text offset → line.
Result: `port/docs/fma-sites.txt` (file:line, function, instruction kind,
count), 1865 source lines.

Cross-check against the DOL (per-function counts, `fma_scan.py compare`):

- objects: 3814 fused instructions in 841 functions; DOL: 3683 in 819
- 810 functions in both, **805 with identical counts**
- the rest are naming or linking artifacts, not missing sites:
  - static/inline helpers emitted into several objects and deduplicated or
    dropped at link (`HSD_PadClamp` 12, `calc_dist_2d_accurate` 8,
    `*_inline`, `commonCall`, ...), which inflates the object total;
  - Master Hand and Crazy Hand share code, and `symbols.txt` names the
    retail copies `ftCh_*` where the objects say `ftMh_*` (or the reverse);
  - `ftColl_8007A06C` 14 vs 20 and `fn_801857C4` 3 vs 7: the object copy
    includes inlined helpers the retail function calls out-of-line.

Rebuild the map (about 1 minute on this host) from a scratch worktree so
`build/` stays untouched:

    python configure.py --sym on --build-dir build-sym
    ninja <all build-sym/GALE01/src/**.o edges that use an mwcc rule>
    .venv/bin/python port/tools/fma_scan.py objs build-sym/GALE01/src \
        --sites port/docs/fma-sites.txt

(The port build needs the matching build's `build/GALE01/include`; link it
into the scratch worktree.)
