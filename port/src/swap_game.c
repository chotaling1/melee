/// Game-specific DAT root types (fighters, stages, items, menus), keyed by
/// public symbol name. Grows as the port loads more of the game.

#include <string.h>

#include <melee/gr/types.h>
#include <melee/mp/types.h>
#include <melee/sc/types.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/lobj.h>
#include <sysdolphin/baselib/mobj.h>

#include <port/endian.h>
#include <port/port.h>
#include <port/swap.h>

#define OK(p) ((p) != NULL && port_in_archive(p))

/// LbRf.dat "lbRefData" (src/melee/lb/lbrefract.c): { u8 count; f32* params }
/// with two f32 parameters per refraction texture.
static void swap_lbRefData(void* addr)
{
    struct {
        u8 count;
        f32* params;
    }* d = addr;
    if (OK(d->params)) {
        port_swap32_array(d->params, (size_t) d->count * 2);
    }
}

/* ---- stages: Gr*.dat (src/melee/gr/grdatfiles.c) ----
 * Layouts follow the decomp types (src/melee/gr/types.h,
 * src/melee/mp/types.h), cross-checked against HSDLib's SBM_Map_Head,
 * SBM_Map_GOBJ, SBM_Coll_Data and SBM_GroundParam. */

#define EACH_NULLTERM(T, arr, v)                                              \
    for (T* const* v##_it = (T* const*) (arr); OK(v##_it) && *v##_it;        \
         v##_it++)                                                            \
        for (T* v = *v##_it; v; v = NULL)

/// "coll_data": vertices, lines and joint groups.
static void swap_coll_data(void* addr)
{
    MapCollData* c = addr;
    int i;
    port_swap_MapCollData(c);
    if (OK(c->verts)) {
        port_swap32_array(c->verts, (size_t) c->vert_count * 2);
    }
    for (i = 0; OK(c->lines) && i < c->line_count; i++) {
        port_swap_MapLine(&c->lines[i]);
    }
    for (i = 0; OK(c->joints) && i < c->joint_count; i++) {
        port_swap_MapJoint(&c->joints[i]);
    }
}

/// "grGroundParam": the GroundParam block plus its per-StKind rows.
static void swap_grGroundParam(void* addr)
{
    GroundParam* g = addr;
    int i;
    port_swap_GroundParam(g);
    for (i = 0; OK(g->stage_params) && i < g->stage_param_count; i++) {
        port_swap_StageParam(&g->stage_params[i]);
    }
}

/// One map model group (HSDLib SBM_Map_GOBJ, 0x34 bytes).
static void swap_map_gobj(struct UnkStageDat_x8_t* m)
{
    int i;
    port_swap_UnkStageDat_x8_t(m);
    port_walk_Joint(m->unk0);
    EACH_NULLTERM(HSD_AnimJoint, m->unk4, a) { port_walk_AnimJoint(a); }
    EACH_NULLTERM(HSD_MatAnimJoint, m->unk8, a) { port_walk_MatAnimJoint(a); }
    EACH_NULLTERM(HSD_ShapeAnimJoint, m->unkC, a)
    {
        port_walk_ShapeAnimJoint(a);
    }
    port_walk_CObjDesc((HSD_CObjDesc*) m->x10);
    port_walk_LightLists(m->x18);
    port_walk_FogDesc(m->x1C);
    for (i = 0; OK(m->unk20) && i < m->unk24; i++) {
        port_swap_GrJoint(&m->unk20[i]);
    }
    /* Second 6-byte collision-link table (three s16 per entry). */
    if (OK(m->x2C)) {
        port_swap16_array(m->x2C, (size_t) m->x30 * 3);
    }
}

/// "map_head".
static void swap_map_head(void* addr)
{
    UnkStageDat* h = addr;
    int i;

    port_swap_UnkStageDat(h);

    /* +0: general points, 0xC each: { HSD_Joint*, {s16 jobj, s16 type}*,
     * s32 count } */
    for (i = 0; OK(h->unk0) && i < h->unk4; i++) {
        u8* gp = (u8*) h->unk0 + i * 0xC;
        void* info = *(void**) (gp + 4);
        port_swap32(gp + 8);
        if (OK(info)) {
            port_swap16_array(info, (size_t) *(s32*) (gp + 8) * 2);
        }
    }

    for (i = 0; OK(h->unk8) && i < h->unkC; i++) {
        swap_map_gobj(&h->unk8[i]);
    }

    for (i = 0; OK(h->unk10) && i < h->unk14; i++) {
        port_walk_Spline(h->unk10[i]);
    }

    /* +18: map lights, 8 each: { HSD_LightDesc*, s32 flags } */
    for (i = 0; OK(h->unk18) && i < h->unk1C; i++) {
        u8* ml = (u8*) h->unk18 + i * 8;
        port_walk_LightDesc(*(HSD_LightDesc**) ml);
        port_swap32(ml + 4);
    }

    for (i = 0; OK(h->unk20) && i < h->unk24; i++) {
        port_swap_GroundShadowEntry(&h->unk20[i]);
    }

    /* +28: material descs (UnkStageDatInternal is an MObjDesc prefix). */
    for (i = 0; OK(h->unk28) && i < h->unk2C; i++) {
        port_walk_MObjDesc((HSD_MObjDesc*) h->unk28[i]);
    }
}

/// Number of words in the array at `addr`: its extent, extended while the
/// words past it are still relocated pointers (the entries of a pointer
/// array that were laid out before their targets shorten the extent).
static size_t array_words(void* addr)
{
    size_t n = port_extent(addr) / 4;
    while (n < 256 && port_is_pointer_word((u8*) addr + n * 4)) {
        n++;
    }
    return n;
}

/// "itemdata": the stage's own items (ground.c Ground_801C0800), a
/// NULL-terminated array of { s32 kind; Article* } (struct GroundItemData)
/// handed to it_8026B40C. Battlefield's is empty (just the terminator).
static void swap_itemdata(void* addr)
{
    size_t i, n = array_words(addr);
    for (i = 0; i < n; i++) {
        s32* e;
        if (!port_is_pointer_word((s32**) addr + i)) {
            continue; /* a plain 0 word is the NULL terminator */
        }
        e = *((s32**) addr + i);
        if (!OK(e)) {
            continue;
        }
        port_swap32(e); /* kind */
        port_swap_item_article(*e, *(void**) (e + 1));
    }
}

/// "ALDYakuAll": item state scripts copied over the stage item article's
/// states (ground.c Ground_801C0800, from index 1). Entry 0 is unused.
static void swap_ald_yaku_all(void* addr)
{
    size_t i, n = array_words(addr);
    for (i = 0; i < n; i++) {
        void* script;
        if (!port_is_pointer_word((void**) addr + i)) {
            continue;
        }
        script = *((void**) addr + i);
        if (OK(script)) {
            port_swap_script(script);
        }
    }
}

/// "yakumono_param": stage hazard parameters; every gr*.c casts it to its
/// own struct, so there is no shared layout. Battlefield's
/// (grbattle.c grBattle_YakumonoParam) is two background color-overlay
/// script pointers. Other stages get their scripts swapped and a warning
/// for the scalar words, which need that stage's struct.
static void swap_yakumono_param(void* addr)
{
    size_t i, n = port_extent(addr) / 4;
    size_t scalars = 0;
    for (i = 0; i < n; i++) {
        void* w = (u8*) addr + i * 4;
        if (port_is_pointer_word(w)) {
            port_swap_script(*(void**) w);
        } else {
            scalars++;
        }
    }
    if (scalars != 0) {
        port_log("yakumono_param: %u untyped scalar words (stage struct "
                 "not implemented)",
                 (unsigned) scalars);
    }
}

static void swap_map_plit(void* addr)
{
    port_walk_LightLists(addr);
}

static void swap_quake_model_set(void* addr)
{
    port_walk_DynamicModelDesc(addr);
}

/// Particle banks are swapped when psInitDataBank runs on them.
static void swap_nothing(void* addr)
{
    (void) addr;
}

typedef struct {
    const char* symbol;
    void (*swap)(void* addr);
} GameRoot;

static const GameRoot roots[] = {
    { "lbRefData", swap_lbRefData },
    { "itPublicData", port_swap_itPublicData },
    { "coll_data", swap_coll_data },
    { "itemdata", swap_itemdata },
    { "ALDYakuAll", swap_ald_yaku_all },
    { "yakumono_param", swap_yakumono_param },
    { "grGroundParam", swap_grGroundParam },
    { "map_head", swap_map_head },
    { "map_plit", swap_map_plit },
    { "quake_model_set", swap_quake_model_set },
    { "map_ptcl", swap_nothing },
    { "map_texg", swap_nothing },
};

int port_swap_game_public(const char* symbol, void* addr)
{
    size_t i;
    size_t n = strlen(symbol);

    /* eff*DataTable: { cmd bank*; tex bank*; EF_EffectDesc[] } where each
     * EF_EffectDesc (src/melee/ef/types.h, 0x14) is { f32 lifetime;
     * StaticModelDesc }, indexed by gfx_id % 1000 (efLib_Create). The banks
     * are swapped by psInitDataBankLocate -> port_swap_ps_banks. */
    if (strncmp(symbol, "eff", 3) == 0 && n > 9 &&
        strcmp(symbol + n - 9, "DataTable") == 0)
    {
        u8* descs = (u8*) addr + 8;
        size_t count = (port_extent(addr) - 8) / 0x14;
        for (i = 0; i < count; i++) {
            u8* d = descs + i * 0x14;
            port_swap32(d);
            port_walk_StaticModelDesc((StaticModelDesc*) (d + 4));
        }
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
