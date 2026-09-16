#!/usr/bin/env python3
"""Resolve addresses printed by the port's crash handler to symbol+file:line.

Usage:
    .venv/bin/python port/tools/symbolize.py port/build/melee 0x0804abcd ...
    <crash output> | .venv/bin/python port/tools/symbolize.py port/build/melee

Uses pyelftools: the ELF symbol table for function names and DWARF line
programs for file:line.
"""

import bisect
import re
import sys

from elftools.elf.elffile import ELFFile


def load(path):
    f = open(path, "rb")
    elf = ELFFile(f)
    funcs = []
    symtab = elf.get_section_by_name(".symtab")
    for sym in symtab.iter_symbols():
        if sym["st_info"]["type"] == "STT_FUNC" and sym["st_size"] > 0:
            funcs.append((sym["st_value"], sym["st_size"], sym.name))
    funcs.sort()
    starts = [s for s, _, _ in funcs]
    return elf, funcs, starts


def lookup_func(funcs, starts, addr):
    i = bisect.bisect_right(starts, addr) - 1
    if i >= 0:
        start, size, name = funcs[i]
        if start <= addr < start + size:
            return name, addr - start
    return None, 0


def build_line_index(elf):
    """Map address -> (file, line) using every CU's line program."""
    if not elf.has_dwarf_info():
        return [], []
    dwarf = elf.get_dwarf_info()
    entries = []
    for cu in dwarf.iter_CUs():
        lp = dwarf.line_program_for_CU(cu)
        if lp is None:
            continue
        header = lp.header
        file_entries = header["file_entry"]
        include_dirs = header["include_directory"]
        prev = None
        for e in lp.get_entries():
            st = e.state
            if st is None:
                continue
            fe = file_entries[st.file - 1] if header.version < 5 else file_entries[st.file]
            d = fe.dir_index
            if header.version < 5:
                dirname = include_dirs[d - 1] if d > 0 else b"."
            else:
                dirname = include_dirs[d]
            fname = b"%s/%s" % (bytes(dirname), bytes(fe.name))
            entries.append((st.address, fname.decode(errors="replace"), st.line))
            if st.end_sequence:
                entries.append((st.address, None, 0))
    entries.sort(key=lambda t: t[0])
    return entries, [a for a, _, _ in entries]


def lookup_line(entries, addrs, addr):
    i = bisect.bisect_right(addrs, addr) - 1
    if i >= 0 and entries[i][1]:
        return "%s:%d" % (entries[i][1].split("/melee-port/")[-1], entries[i][2])
    return "?"


def main():
    path = sys.argv[1]
    args = sys.argv[2:]
    if not args:
        args = re.findall(r"0x[0-9a-fA-F]{6,8}", sys.stdin.read())
    elf, funcs, starts = load(path)
    entries, addrs = build_line_index(elf)
    for a in args:
        addr = int(a, 16)
        name, off = lookup_func(funcs, starts, addr)
        # Return addresses point after the call; look up the call itself.
        where = lookup_line(entries, addrs, addr - 1 if off else addr)
        print("0x%08x  %s+0x%x  %s" % (addr, name or "?", off, where))


if __name__ == "__main__":
    main()
