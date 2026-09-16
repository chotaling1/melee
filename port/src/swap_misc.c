/// Remaining DAT roots loaded around a match: rumble patterns, trophy
/// tables, the title logo sprite, background-flash color animations, and
/// data that is raw bytes by design (SIS text, memory card icons).

#include <string.h>

#include <sysdolphin/baselib/sobjlib.h>
#include <sysdolphin/baselib/tobj.h>

#include <port/endian.h>
#include <port/port.h>
#include <port/swap.h>

#define OK(p) ((p) != NULL && port_in_archive(p))

static inline void* ptr_at(void* obj, size_t off)
{
    return *(void**) ((u8*) obj + off);
}

/// LbRb.dat "lbRumbleData" (lb_013B.c): { u16* pattern; u8; u8; u8 pad[2] }
/// rows. Patterns are u16 commands decoded by HSD_PadRumble (rumble.c).
static void swap_lbRumbleData(void* addr)
{
    size_t i, n = port_extent(addr) / 8;
    for (i = 0; i < n; i++) {
        void* pattern = ptr_at(addr, i * 8);
        if (OK(pattern)) {
            port_swap16_array(pattern, port_extent(pattern) / 2);
        }
    }
}

/// LbBf.dat "lbBgFlashColAnimData": color animation rows { script*; u8
/// priority; u8 pad[3] } like ftLoadCommonData's (lb_800144C8).
static void swap_lbBgFlashColAnimData(void* addr)
{
    size_t i, n = port_extent(addr) / 8;
    for (i = 0; i < n; i++) {
        void* script = ptr_at(addr, i * 8);
        if (OK(script)) {
            port_swap_script(script);
        }
    }
}

/// "TitleMark_sobjdesc": HSD_SObjDesc { HSD_ImageDesc*; HSD_TlutDesc* }.
/// Pixel and palette bytes stay big-endian for the renderer.
static void swap_sobjdesc(void* addr)
{
    size_t i, n = port_extent(addr) / sizeof(HSD_SObjDesc);
    for (i = 0; i < n; i++) {
        HSD_SObjDesc* d = (HSD_SObjDesc*) ((u8*) addr + i * sizeof(*d));
        if (OK(d->image)) {
            port_swap_HSD_ImageDesc(d->image);
        }
        if (OK(d->tlut)) {
            port_swap_HSD_TlutDesc(d->tlut);
        }
    }
}

/// Array of fixed-size elements with a generated per-element swap.
static void each(void* addr, size_t size, void (*swap)(void*))
{
    size_t i, n = port_extent(addr) / size;
    for (i = 0; i < n; i++) {
        swap((u8*) addr + i * size);
    }
}

/// TyDatai.usd trophy tables (toy.c loadTrophyMetadata).
static void swap_tyTrophyData(void* addr)
{
    each(addr, 0x24, port_swap_TrophyData);
}

static void swap_tyNameData(void* addr)
{
    each(addr, 0xC, port_swap_ToyNameData);
}

static void swap_tyDspEntry(void* addr)
{
    each(addr, 0x10, port_swap_TyDspEntry);
}

/// s16 lists terminated by -1.
static void swap_s16_list(void* addr)
{
    port_swap16_array(addr, port_extent(addr) / 2);
}

/// Pointer tables to byte-coded data: SIS text (sislib parses bytes), and
/// memory card banner/icon images (written to the card as is).
static void swap_raw(void* addr)
{
    (void) addr;
}

typedef struct {
    const char* symbol;
    void (*swap)(void* addr);
} MiscRoot;

static const MiscRoot roots[] = {
    { "lbRumbleData", swap_lbRumbleData },
    { "lbBgFlashColAnimData", swap_lbBgFlashColAnimData },
    { "TitleMark_sobjdesc", swap_sobjdesc },
    { "tyInitModelTbl", swap_tyTrophyData },
    { "tyInitModelDTbl", swap_tyTrophyData },
    { "tyModelSortTbl", swap_tyNameData },
    { "tyExpDifferentTbl", swap_s16_list },
    { "tyNoGetUsTbl", swap_s16_list },
    { "tyDisplayModelTbl", swap_tyDspEntry },
    { "tyDisplayModelUsTbl", swap_tyDspEntry },
    { "MemCardIconData", swap_raw },
};

int port_swap_misc_public(const char* symbol, void* addr)
{
    size_t i;
    if (strncmp(symbol, "SIS_", 4) == 0) {
        swap_raw(addr);
        return 1;
    }
    for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        if (strcmp(symbol, roots[i].symbol) == 0) {
            roots[i].swap(addr);
            return 1;
        }
    }
    return 0;
}
