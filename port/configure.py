#!/usr/bin/env python3
"""Generate port/build.ninja for the native PC port.

This is deliberately separate from the top-level configure.py so the
byte-matching GameCube build is never affected.

Usage (from the repo root):
    .venv/bin/python port/configure.py
    .venv/bin/ninja -C port

Output: port/build/melee (static 32-bit x86 Linux executable).
"""

import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT = ROOT / "port"
PYTHON = sys.executable
TARGET = os.environ.get("PORT_TARGET", "x86-linux-musl")
WINDOWS = "windows" in TARGET
# The default (Linux) target keeps port/build and port/build.ninja; other
# targets get their own build directory and ninja file.
BUILD_DIR = "build" if TARGET == "x86-linux-musl" else f"build-{TARGET}"
NINJA_FILE = "build.ninja" if BUILD_DIR == "build" else f"{BUILD_DIR}.ninja"
EXE = "melee.exe" if WINDOWS else "melee"

# Compiler: Zig's bundled clang. Note: `-fsyntax-only` and `-include` are
# broken under zig (bogus FileNotFound), always compile with `-c -o`.
CC = f"{PYTHON} -m ziglang cc"

CFLAGS = [
    f"-target {TARGET}",
    "-std=gnu99",
    "-g",
    "-O1",
    # Keep EBP chains so the crash handler can print a backtrace.
    "-fno-omit-frame-pointer",
    # Silence the decomp's warnings; they are tuned for MWCC.
    "-w",
    "-Wno-error=incompatible-function-pointer-types",
    # The game does a lot of type punning through DAT structs.
    "-fno-strict-aliasing",
    "-fwrapv",
    # Float determinism: single-precision SSE math, no fused multiply-add
    # (x87 would give 80-bit intermediates, FMA would differ from PPC
    # non-fused paths). PPC fmadd matching is a separate, later problem.
    "-msse2",
    "-mfpmath=sse",
    "-ffp-contract=off",
    # Game float math comes from MSL (src/MSL, port/src/math_port.c), never
    # the host libm: clang must not fold or rewrite these calls either.
    *[f"-fno-builtin-{fn}" for fn in
      ("sinf", "cosf", "tanf", "atanf", "fmodf", "logf")],
    # Same defines as the matching build ...
    "-DVERSION_GALE01",
    "-DBUILD_VERSION=0",
    "-DNDEBUG=1",
    # ... plus the port switch used for #ifdef'd source changes.
    "-DMELEE_PORT=1",
    # Runtime/platform.h typedefs these for PPC; use the host's instead.
    "-Duintptr_t=__UINTPTR_TYPE__",
    "-Dintptr_t=__INTPTR_TYPE__",
    # port/include first: it overrides stdbool.h (bool must be a 4-byte int).
    f"-I{ROOT}/port/include",
    # Build-time generated headers (port_gen/*.h).
    f"-I{ROOT}/port/{BUILD_DIR}/gen",
    f"-isystem {ROOT}/src/MSL",
    f"-I{ROOT}/src",
    f"-I{ROOT}/libs/dolphin/include",
    f"-I{ROOT}/build/GALE01/include",
    f"-I{ROOT}/libs/dolphin/src",
]

# Windows (mingw) defaults to MSVC bitfield layout, where a bitfield run of
# one base type takes a whole unit of that type (u32 flags : 1 ... ; u8 x;
# is 8 bytes instead of 4). Game structs, and the swap walkers generated from
# the Linux build's DWARF, assume the GCC layout both Linux and MWCC-era
# decomp code rely on, so game code uses it on Windows too. Host files that
# include <windows.h> (*_win32.c) keep the platform ABI.
GAME_CFLAGS = ["-mno-ms-bitfields"] if WINDOWS else []

LDFLAGS = [
    f"-target {TARGET}",
    # Linux: fully static (no 32-bit loader on the build host). Windows:
    # MEM1 is mapped at 0x80000000, so the process needs a 4 GB address
    # space (large-address-aware, 64-bit Windows).
    # large-address-aware is set after linking (port/tools/pe_laa.py).
    # Windows: 8 MB stack like the Linux default (the default 1 MB is
    # smaller than some game code paths expect).
    *(["-Wl,--stack,8388608"] if WINDOWS else ["-static"]),
    "-g",
]

# Source roots that go into the port, relative to the repo root.
SOURCE_DIRS = [
    "src/melee",
    "src/sysdolphin",
    "port/src",
]

# Individual SDK sources from libs/dolphin that are plain, portable C.
# (Most of the SDK touches hardware registers or uses PPC inline asm; those
# subsystems are re-implemented or stubbed in port/src instead.)
EXTRA_SOURCES = [
    "libs/dolphin/src/dolphin/os/OSAlloc.c",  # heaps: __OSCurrHeap & co.
    "libs/dolphin/src/dolphin/os/OSArena.c",  # arena lo/hi bump allocator
    "libs/dolphin/src/dolphin/mtx/mtx44.c",  # MTXFrustum/Perspective/Ortho
    # MSL float math as on console (host libm differs per OS and from PPC).
    "src/MSL/trigf.c",  # sinf, cosf, tanf
    "src/MSL/math.c",  # logf
    "src/MSL/math_data.c",  # sin/cos polynomial, ln tables
]

# Files that are PPC/MSL-only and have a port replacement in port/src.
EXCLUDE = set()


def collect_sources():
    out = []
    for d in SOURCE_DIRS:
        for p in sorted((ROOT / d).rglob("*.c")):
            rel = p.relative_to(ROOT).as_posix()
            if rel in EXCLUDE:
                continue
            # Host-specific platform files: *_posix.c or *_win32.c.
            if rel.endswith("_posix.c") and WINDOWS:
                continue
            if rel.endswith("_win32.c") and not WINDOWS:
                continue
            out.append(rel)
    out += EXTRA_SOURCES
    return out


def main():
    sources = collect_sources()
    lines = []
    w = lines.append
    w("# Generated by port/configure.py. Do not edit.")
    w("ninja_required_version = 1.3")
    w(f"builddir = {BUILD_DIR}")
    w(f"cc = {CC}")
    w("cflags = " + " ".join(CFLAGS))
    w("ldflags = " + " ".join(LDFLAGS))
    w("")
    w("rule cc")
    w("  command = $cc $cflags -MMD -MF $out.d -c -o $out $in")
    w("  depfile = $out.d")
    w("  deps = gcc")
    w("  description = CC $in")
    w("")
    w("rule link")
    if WINDOWS:
        w(f"  command = $cc $ldflags -o $out $in && {PYTHON} {PORT}/tools/pe_laa.py $out")
    else:
        w("  command = $cc $ldflags -o $out $in")
    w("  description = LINK $out")
    w("")
    # LSB-first script command layouts, derived from the decomp header.
    gen_cmd = "$builddir/gen/port_gen/lb_cmd.h"
    w("rule gen_cmd_layouts")
    w(f"  command = {PYTHON} {PORT}/tools/gen_cmd_layouts.py $in $out")
    w("  description = GEN $out")
    w("")
    w(f"build {gen_cmd}: gen_cmd_layouts {ROOT}/src/melee/lb/types.h"
      f" | {PORT}/tools/gen_cmd_layouts.py")
    w("")
    objs = []
    for src in sources:
        obj = "$builddir/obj/" + src[:-2] + ".o"
        objs.append(obj)
        w(f"build {obj}: cc {ROOT}/{src} || {gen_cmd}")
        if GAME_CFLAGS and not src.endswith("_win32.c"):
            w("  cflags = $cflags " + " ".join(GAME_CFLAGS))
    w("")
    w(f"build $builddir/{EXE}: link " + " ".join(objs))
    w(f"default $builddir/{EXE}")
    w("")
    (PORT / NINJA_FILE).write_text("\n".join(lines))
    print(f"port/{NINJA_FILE}: {len(sources)} sources -> port/{BUILD_DIR}/{EXE}")


if __name__ == "__main__":
    main()
