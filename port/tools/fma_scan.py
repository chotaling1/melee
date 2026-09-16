#!/usr/bin/env python3
"""Find fused multiply-add instructions (PORT-010, see port/docs/fp-parity.md).

Game code is built by MWCC with -fp_contract on, which emits PowerPC
fmadd/fmsub/fnmadd/fnmsub. The port computes those expressions unfused
unless they are made explicit, so this tool locates every fused site and
counts fused instructions per function.

Usage (from the repo root):

  fma_scan.py dol [--json OUT]
      Count per function in orig/GALE01/sys/main.dol (names from
      config/GALE01/symbols.txt).

  fma_scan.py objs OBJDIR [--sites OUT] [--json OUT]
      Map every fused instruction in MWCC objects built with "-sym on" to
      file:line through their DWARF 1 .line tables. Build the objects in a
      scratch build directory, e.g. in a separate worktree:
          python configure.py --sym on --build-dir build-sym
          ninja <every build-sym/GALE01/src/**.o built by an mwcc rule>
      (-sym on adds debug sections only; the scan cross-checks per-function
      counts against the DOL.)

  fma_scan.py port ELF [--json OUT]
      Count fused x86 instructions (vfmadd*, vfmsub*, vfnmadd*, vfnmsub*)
      per function in a port build (needs capstone in .venv).

  fma_scan.py compare A.json B.json
      Per-function comparison of two count files.
"""

import argparse
import bisect
import collections
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent

# PowerPC A-form: primary opcode 59 (single) / 63 (double), XO in bits 26..30.
KINDS = {28: "fmsub", 29: "fmadd", 30: "fnmsub", 31: "fnmadd"}


def decode(word):
    op = word >> 26
    if op not in (59, 63):
        return None
    kind = KINDS.get((word >> 1) & 0x1F)
    if kind is None:
        return None
    return kind + ("s" if op == 59 else "")


def scan_dol(args):
    dol = (ROOT / "orig/GALE01/sys/main.dol").read_bytes()
    offs = struct.unpack(">18I", dol[0:0x48])
    addrs = struct.unpack(">18I", dol[0x48:0x90])
    sizes = struct.unpack(">18I", dol[0x90:0xD8])
    funcs = []
    for line in (ROOT / "config/GALE01/symbols.txt").read_text().splitlines():
        m = re.match(r"(\S+) = \.\w+:0x([0-9A-Fa-f]+); //.*type:function", line)
        if m:
            funcs.append((int(m.group(2), 16), m.group(1)))
    funcs.sort()
    starts = [a for a, _ in funcs]
    counts = collections.Counter()
    kinds = collections.Counter()
    for i in range(7):  # text sections
        code = dol[offs[i]:offs[i] + sizes[i]]
        for k in range(0, len(code) - 3, 4):
            kind = decode(struct.unpack_from(">I", code, k)[0])
            if kind:
                j = bisect.bisect_right(starts, addrs[i] + k) - 1
                counts[funcs[j][1] if j >= 0 else "?"] += 1
                kinds[kind] += 1
    print(f"main.dol: {sum(counts.values())} fused instructions in "
          f"{len(counts)} functions {dict(kinds)}")
    return dict(counts)


def line_tables(elf, sec_index_of_sym):
    """DWARF 1 .line: chunks of {u32 length; u32 base (relocated to a text
    section); {u32 line; u16 col; u32 offset}[]}. Returns
    {text section index: sorted [(offset, line)]}."""
    sec = elf.get_section_by_name(".line")
    rela = elf.get_section_by_name(".rela.line")
    if sec is None or rela is None:
        return {}
    data = sec.data()
    base_sec = {}
    for r in rela.iter_relocations():
        base_sec[r["r_offset"]] = sec_index_of_sym(r["r_info_sym"])
    tables = collections.defaultdict(list)
    pos = 0
    while pos + 8 <= len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        if length < 8:
            break
        shndx = base_sec.get(pos + 4)
        for e in range(pos + 8, pos + length - 9, 10):
            line, _col, off = struct.unpack_from(">IHI", data, e)
            if shndx is not None and line:
                tables[shndx].append((off, line))
        pos += length
    for v in tables.values():
        v.sort()
    return tables


def scan_objs(args):
    from elftools.elf.elffile import ELFFile

    objdir = Path(args.path)
    counts = collections.Counter()
    sites = []
    for obj in sorted(objdir.rglob("*.o")):
        elf = ELFFile(obj.open("rb"))
        symtab = elf.get_section_by_name(".symtab")
        syms = list(symtab.iter_symbols())
        tables = line_tables(elf, lambda i: syms[i]["st_shndx"])
        src = obj.relative_to(objdir).with_suffix(".c").as_posix()
        src = src.split("src/", 1)[-1]
        for shndx, sec in enumerate(elf.iter_sections()):
            if sec.name != ".text" or sec["sh_size"] == 0:
                continue
            funcs = sorted((s["st_value"], s["st_size"], s.name) for s in syms
                           if s["st_info"]["type"] == "STT_FUNC"
                           and s["st_shndx"] == shndx)
            fstarts = [f[0] for f in funcs]
            lines = tables.get(shndx, [])
            lstarts = [o for o, _ in lines]
            code = sec.data()
            for k in range(0, len(code) - 3, 4):
                kind = decode(struct.unpack_from(">I", code, k)[0])
                if not kind:
                    continue
                j = bisect.bisect_right(fstarts, k) - 1
                fn = funcs[j][2] if j >= 0 else "?"
                li = bisect.bisect_right(lstarts, k) - 1
                line = lines[li][1] if li >= 0 else 0
                counts[fn] += 1
                sites.append((src, line, fn, kind))
    print(f"objects: {sum(counts.values())} fused instructions in "
          f"{len(counts)} functions, {len(set((s, l) for s, l, _, _ in sites))} "
          f"source lines")
    if args.sites:
        per_line = collections.Counter(sites)
        with open(args.sites, "w") as f:
            f.write("# Fused multiply-add sites in MWCC game code "
                    "(port/tools/fma_scan.py objs).\n"
                    "# file:line function kind count\n")
            for (src, line, fn, kind), n in sorted(per_line.items()):
                f.write(f"{src}:{line} {fn} {kind} {n}\n")
    return dict(counts)


def scan_port(args):
    import capstone
    from elftools.elf.elffile import ELFFile

    elf = ELFFile(open(args.path, "rb"))
    text = elf.get_section_by_name(".text")
    base, data = text["sh_addr"], text.data()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    counts = {}
    for s in elf.get_section_by_name(".symtab").iter_symbols():
        a, size = s["st_value"], s["st_size"]
        if s["st_info"]["type"] != "STT_FUNC" or not size:
            continue
        if not base <= a < base + len(data):
            continue
        n = sum(1 for i in md.disasm(data[a - base:a - base + size], a)
                if i.mnemonic.startswith(("vfmadd", "vfmsub", "vfnmadd",
                                          "vfnmsub")))
        if n:
            counts[s.name] = n
    print(f"{args.path}: {sum(counts.values())} fused instructions in "
          f"{len(counts)} functions")
    return counts


def compare(args):
    a = json.load(open(args.path))
    b = json.load(open(args.other))
    both = set(a) & set(b)
    eq = sum(1 for f in both if a[f] == b[f])
    print(f"A: {len(a)} functions / {sum(a.values())}; B: {len(b)} / "
          f"{sum(b.values())}; in both {len(both)}, equal counts {eq}; "
          f"only A {len(set(a) - set(b))}; only B {len(set(b) - set(a))}")
    for f in sorted(set(a) | set(b)):
        if a.get(f, 0) != b.get(f, 0):
            print(f"  {f}: {a.get(f, 0)} vs {b.get(f, 0)}")


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("mode", choices=["dol", "objs", "port", "compare"])
    p.add_argument("path", nargs="?")
    p.add_argument("other", nargs="?")
    p.add_argument("--json")
    p.add_argument("--sites")
    args = p.parse_args()
    if args.mode == "compare":
        compare(args)
        return
    counts = {"dol": scan_dol, "objs": scan_objs, "port": scan_port}[
        args.mode](args)
    if args.json:
        json.dump(counts, open(args.json, "w"), indent=0, sort_keys=True)


if __name__ == "__main__":
    main()
