#!/usr/bin/env python3
"""Generate byte-swap functions for DAT descriptor structs from DWARF.

Usage (from the repo root, after a port build):
    .venv/bin/python port/tools/gen_swap.py port/build/melee

Reads the struct layouts the port was compiled with (so they always match
the headers), and writes port/src/swap_gen.c containing, for every type in
TYPES:

    void port_swap_<Type>(void* p);

which converts each multi-byte scalar member (ints, floats, enums) of one
struct instance in place via port_swap16/32. Pointer members are skipped:
port_archive_swap already converted every relocated word, and NULL is the
same in both byte orders. Nested structs and arrays are expanded inline.

Bitfields are remapped, not just swapped: MWCC (big-endian) allocates
bitfields from the MSB of each storage unit, clang on x86 from the LSB.
Both pack a run of same-typed bitfields sequentially, so a field at LSB
position `pos` (from DWARF's DW_AT_data_bit_offset) sits at MSB position
`pos` in the disc data. Each unit is read big-endian and rebuilt with every
field moved to its host position.

Members the generator cannot convert safely are reported, not guessed:
  - unions with mixed-size members (the active member depends on data)
  - 8-byte scalars
Write those by hand in port/src/swap_port.c.
"""

import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parent.parent.parent
OUT = ROOT / "port/src/swap_gen.c"

# Descriptor structs loaded straight out of DAT archives.
TYPES = [
    "HSD_WObjDesc",
    "HSD_CameraDescCommon",
    "HSD_CameraDescFrustum",
    "HSD_CameraDescPerspective",
    "HSD_Joint",
    "HSD_DObjDesc",
    "HSD_MObjDesc",
    "HSD_Material",
    "HSD_PEDesc",
    "HSD_TObjDesc",
    "HSD_ImageDesc",
    "HSD_TlutDesc",
    "HSD_TObjTevDesc",
    "HSD_PObjDesc",
    "HSD_ShapeSetDesc",
    "HSD_ShapeAnim",
    "HSD_ShapeAnimJoint",
    "HSD_ShapeAnimDObj",
    "HSD_EnvelopeDesc",
    "HSD_VtxDescList",
    "HSD_AObjDesc",
    "HSD_FObjDesc",
    "HSD_AnimJoint",
    "HSD_MatAnimJoint",
    "HSD_MatAnim",
    "HSD_TexAnim",
    "HSD_RenderAnim",
    "HSD_LightDesc",
    "HSD_LightPoint",
    "HSD_LightSpot",
    "HSD_LightAttn",
    "HSD_LightAnim",
    "HSD_FogDesc",
    "HSD_FogAdjDesc",
    "HSD_RObjDesc",
    "HSD_RObjAnimJoint",
    "HSD_CameraAnim",
    "HSD_WObjAnim",
    "HSD_ExpDesc",
    "HSD_ByteCodeExpDesc",
    "HSD_RvalueList",
    "HSD_LightPointDesc",
    "HSD_LightSpotDesc",
    "HSD_TexLODDesc",
    "HSD_Spline",
    # Stages (src/melee/gr/types.h, src/melee/mp/types.h)
    "GroundParam",
    "StageParam",
    "MapCollData",
    "MapLine",
    "MapJoint",
    "UnkStageDat",
    "UnkStageDat_x8_t",
    "GroundShadowEntry",
    "GrJoint",
]


class Layout:
    def __init__(self, dwarf):
        self.dwarf = dwarf
        self.structs = {}  # name -> DIE (first complete definition)
        for cu in dwarf.iter_CUs():
            for die in cu.iter_DIEs():
                if die.tag not in ("DW_TAG_structure_type", "DW_TAG_typedef",
                                   "DW_TAG_union_type"):
                    continue
                name = die.attributes.get("DW_AT_name")
                if not name:
                    continue
                name = name.value.decode()
                if die.tag == "DW_TAG_typedef":
                    target = die.get_DIE_from_attribute("DW_AT_type")
                    if target.tag not in ("DW_TAG_structure_type",
                                          "DW_TAG_union_type"):
                        continue
                    if "DW_AT_declaration" in target.attributes:
                        continue
                    self.structs.setdefault(name, target)
                elif "DW_AT_declaration" not in die.attributes:
                    self.structs.setdefault(name, die)

    @staticmethod
    def strip(die):
        while die.tag in ("DW_TAG_typedef", "DW_TAG_const_type",
                          "DW_TAG_volatile_type"):
            die = die.get_DIE_from_attribute("DW_AT_type")
        return die

    def emit(self, die, base, path, out, problems):
        """Emit swap statements for the type `die` located at p+base."""
        die = self.strip(die)
        tag = die.tag
        if tag == "DW_TAG_pointer_type":
            return
        if tag in ("DW_TAG_base_type", "DW_TAG_enumeration_type"):
            size = die.attributes["DW_AT_byte_size"].value
            if size == 1:
                return
            if size == 2:
                out.append(f"    port_swap16((u8*) p + 0x{base:X}); /* {path} */")
            elif size == 4:
                out.append(f"    port_swap32((u8*) p + 0x{base:X}); /* {path} */")
            else:
                problems.append(f"{path}: {size}-byte scalar")
            return
        if tag == "DW_TAG_array_type":
            elem = die.get_DIE_from_attribute("DW_AT_type")
            count = 1
            dims = [c for c in die.iter_children()
                    if c.tag == "DW_TAG_subrange_type"]
            for d in dims:
                if "DW_AT_count" in d.attributes:
                    count *= d.attributes["DW_AT_count"].value
                elif "DW_AT_upper_bound" in d.attributes:
                    count *= d.attributes["DW_AT_upper_bound"].value + 1
                else:
                    problems.append(f"{path}: flexible array")
                    return
            esize = self.size(elem)
            if esize is None:
                problems.append(f"{path}: array of unsized type")
                return
            for i in range(count):
                self.emit(elem, base + i * esize, f"{path}[{i}]", out, problems)
            return
        if tag == "DW_TAG_union_type":
            members = [self.strip(m.get_DIE_from_attribute("DW_AT_type"))
                       for m in die.iter_children() if m.tag == "DW_TAG_member"]
            if all(m.tag == "DW_TAG_pointer_type" for m in members):
                return  # pointers: already swapped via relocations
            if all(self.size(m) == 4 and m.tag in (
                    "DW_TAG_pointer_type", "DW_TAG_base_type",
                    "DW_TAG_enumeration_type") for m in members):
                # One 4-byte word whatever the active member is. If it holds
                # a pointer, the relocation pass already claimed it and this
                # swap is a no-op.
                out.append(f"    port_swap32((u8*) p + 0x{base:X}); /* {path} (union) */")
                return
            problems.append(f"{path}: union")
            return
        if tag == "DW_TAG_structure_type":
            units = {}  # (byte offset, unit bytes) -> [(pos, size, name)]
            for m in die.iter_children():
                if m.tag != "DW_TAG_member":
                    continue
                mname = m.attributes.get("DW_AT_name")
                mname = mname.value.decode() if mname else "<anon>"
                mtype = m.get_DIE_from_attribute("DW_AT_type")
                if "DW_AT_bit_size" in m.attributes:
                    ubytes = self.size(mtype)
                    ubits = ubytes * 8
                    boff = m.attributes["DW_AT_data_bit_offset"].value
                    ustart = (boff // ubits) * ubytes
                    pos = boff - ustart * 8
                    size = m.attributes["DW_AT_bit_size"].value
                    units.setdefault((base + ustart, ubytes), []).append(
                        (pos, size, f"{path}.{mname}"))
                    continue
                off = m.attributes.get("DW_AT_data_member_location")
                off = off.value if off else 0
                self.emit(mtype, base + off, f"{path}.{mname}", out, problems)
            for (uoff, ubytes), fields in sorted(units.items()):
                self.emit_bitfield_unit(uoff, ubytes, fields, out)
            return
        problems.append(f"{path}: unhandled {tag}")

    @staticmethod
    def emit_bitfield_unit(uoff, ubytes, fields, out):
        ubits = ubytes * 8
        names = ", ".join(n for _, _, n in fields)
        ctype = {1: "u8", 2: "u16", 4: "u32"}[ubytes]
        out.append(f"    if (port_claim((u8*) p + 0x{uoff:X}, {ubytes})) {{ /* bitfields: {names} */")
        out.append(f"        u8* b = (u8*) p + 0x{uoff:X};")
        be = " | ".join(f"((u32) b[{i}] << {8 * (ubytes - 1 - i)})"
                        for i in range(ubytes))
        out.append(f"        u32 be = {be};")
        out.append("        u32 host = 0;")
        for pos, size, name in fields:
            mask = (1 << size) - 1
            shift = ubits - pos - size
            out.append(f"        host |= ((be >> {shift}) & 0x{mask:X}u) << {pos};")
        out.append(f"        *({ctype}*) b = ({ctype}) host;")
        out.append("    }")

    def size(self, die):
        die = self.strip(die)
        if "DW_AT_byte_size" in die.attributes:
            return die.attributes["DW_AT_byte_size"].value
        if die.tag == "DW_TAG_array_type":
            elem = die.get_DIE_from_attribute("DW_AT_type")
            es = self.size(elem)
            n = 1
            for d in die.iter_children():
                if d.tag == "DW_TAG_subrange_type":
                    if "DW_AT_count" in d.attributes:
                        n *= d.attributes["DW_AT_count"].value
                    elif "DW_AT_upper_bound" in d.attributes:
                        n *= d.attributes["DW_AT_upper_bound"].value + 1
            return es * n if es is not None else None
        return None


def main():
    elf = ELFFile(open(sys.argv[1], "rb"))
    layout = Layout(elf.get_dwarf_info())
    lines = [
        "// Generated by port/tools/gen_swap.py from the port build's DWARF.",
        "// Do not edit; hand-written swaps live in port/src/swap_port.c.",
        "",
        "#include <port/endian.h>",
        "",
    ]
    report = []
    for t in TYPES:
        die = layout.structs.get(t)
        if die is None:
            report.append(f"{t}: not found in DWARF")
            continue
        out, problems = [], []
        layout.emit(die, 0, t, out, problems)
        size = layout.size(die)
        lines.append(f"/* {t}: 0x{size:X} bytes */")
        lines.append(f"void port_swap_{t}(void* p)")
        lines.append("{")
        lines += out
        for pr in problems:
            lines.append(f"    /* NOT SWAPPED: {pr} */")
        lines.append("}")
        lines.append("")
        report += problems
    OUT.write_text("\n".join(lines))
    print(f"{OUT.relative_to(ROOT)}: {len(TYPES)} types")
    for r in report:
        print("  !", r)


if __name__ == "__main__":
    main()
