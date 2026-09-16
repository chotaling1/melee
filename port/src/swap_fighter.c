/// Fighter DAT roots: Pl<Xx>.dat "ftData<Name>", PlCo.dat
/// "ftLoadCommonData", PdPm.dat "plLoadCommonData".
///
/// Layouts follow the decomp (src/melee/ft/types.h, ftdata.c) cross-checked
/// against HSDLib's SBM_FighterData and friends (HSDRaw/Melee/Pl). Many of
/// these structs are only partly named in the decomp, so most objects are
/// converted with port_swap32_extent(): every 4-byte word of the object up
/// to the next pointer target. That is exact for objects made only of
/// f32/s32/u32 and pointers (relocated pointer words are already claimed
/// and skipped). Objects with 8/16-bit fields are handled explicitly.

#include <string.h>

#include <melee/ft/types.h>
#include <melee/it/types.h>

#include <port/endian.h>
#include <port/port.h>
#include <port/swap.h>

#define OK(p) ((p) != NULL && port_in_archive(p))

/// Pointer stored at offset `off` of object `obj` (already host order).
static inline void* ptr_at(void* obj, size_t off)
{
    return *(void**) ((u8*) obj + off);
}

static inline u32 u32_at(void* obj, size_t off)
{
    return *(u32*) ((u8*) obj + off);
}

/// All-word object.
static void words(void* p)
{
    if (OK(p)) {
        port_swap32_extent(p);
    }
}

/// Array of pointers to all-word objects, bounded by the array's extent.
static void word_ptr_array(void* p)
{
    size_t i, n;
    if (!OK(p)) {
        return;
    }
    n = port_extent(p) / 4;
    for (i = 0; i < n; i++) {
        words(ptr_at(p, i * 4));
    }
}

/// Subaction / color-animation event script. Every command is built from
/// 32-bit words (see CmdUnion in src/melee/lb/types.h), so the whole script
/// is converted word by word; embedded pointers (goto/subroutine targets)
/// are relocated words: skipped, and followed to convert their targets. Bitfield commands are read through
/// the port's LSB-first declarations (port/include/port/lb_cmd.h).
void port_swap_script(void* p)
{
    size_t i, n;
    if (!OK(p) || port_is_claimed(p)) {
        return; /* NULL, not archive data, or already converted */
    }
    words(p);
    /* Goto/subroutine targets are separate objects that only other
     * scripts point at: follow every pointer word. */
    n = port_extent(p) / 4;
    for (i = 0; i < n; i++) {
        void* w = (u8*) p + i * 4;
        if (port_is_pointer_word(w)) {
            port_swap_script(*(void**) w);
        }
    }
}

/* ---- ftData (Pl<Xx>.dat) --------------------------------------------- */

/// +08 model lookup tables (HSDLib SBM_PlayerModelLookupTables, 0x18):
/// { s32 n_vis; CostumeVisTable* vis; s32 n_mat; MatLookup* mat;
///   u8 bones[5] }.
static void swap_model_lookup(void* p)
{
    void* vis;
    void* mat;
    size_t i, j, k;

    if (!OK(p)) {
        return;
    }
    port_swap32(p);
    port_swap32((u8*) p + 8);
    vis = ptr_at(p, 4);
    mat = ptr_at(p, 0xC);

    /* n_vis entries of { LookupTable* high, low, metal, metal_main }, each
     * an array of { s32 count; LookupEntry* }, each entry
     * { s32 count; u8* parts }. */
    for (i = 0; OK(vis) && i < u32_at(p, 0); i++) {
        void* costume = (u8*) vis + i * 0x10;
        for (j = 0; j < 4; j++) {
            void* tables = ptr_at(costume, j * 4);
            size_t ntables;
            if (!OK(tables)) {
                continue;
            }
            ntables = port_extent(tables) / 8;
            for (k = 0; k < ntables; k++) {
                void* table = (u8*) tables + k * 8;
                void* entries;
                size_t e;
                port_swap32(table);
                entries = ptr_at(table, 4);
                for (e = 0; OK(entries) && e < u32_at(table, 0); e++) {
                    port_swap32((u8*) entries + e * 8); /* u8 array: as is */
                }
            }
        }
    }
    /* n_mat pointers to u16 arrays. */
    for (i = 0; OK(mat) && i < u32_at(p, 8); i++) {
        void* shorts = ptr_at(mat, i * 4);
        if (OK(shorts)) {
            port_swap16_array(shorts, port_extent(shorts) / 2);
        }
    }
}

/// +0C / +14 action tables: Fighter_WaitAnimData (0x18) rows of
/// { char* anim_name; u32 anim_offset; u32 anim_size; script*;
///   u32 flags; u32 runtime }.
static void swap_action_table(Fighter_WaitAnimData* t)
{
    size_t i, n;
    if (!OK(t)) {
        return;
    }
    n = port_extent(t) / sizeof(*t);
    for (i = 0; i < n; i++) {
        port_swap32_array(&t[i], sizeof(*t) / 4);
        if (OK(t[i].xC)) {
            port_swap_script(t[i].xC);
        }
    }
}

/// +1C model part animations: pointer array to { u16 part; u16 count;
/// u8* parts; HSD_AnimJoint** anims }.
static void swap_part_anims(void* p)
{
    size_t i, n;
    if (!OK(p)) {
        return;
    }
    n = port_extent(p) / 4;
    for (i = 0; i < n; i++) {
        ftData_x1C* e = ptr_at(p, i * 4);
        void** anims;
        size_t a, na;
        if (!OK(e)) {
            continue;
        }
        port_swap16(&e->x0);
        port_swap16(&e->x2);
        anims = (void**) e->x8;
        if (!OK(anims)) {
            continue;
        }
        na = port_extent(anims) / 4;
        for (a = 0; a < na; a++) {
            port_walk_AnimJoint(anims[a]);
        }
    }
}

/// +2C physics (HSDLib SBM_PhysicsGroup): { s32 n; DynamicDesc* d;
/// s32 n_hit; HitBubble* h; s32** bone_apply }.
static void swap_physics(void* p)
{
    void* descs;
    size_t i;
    if (!OK(p)) {
        return;
    }
    words(p);
    descs = ptr_at(p, 4);
    for (i = 0; OK(descs) && i < u32_at(p, 0); i++) {
        void* d = (u8*) descs + i * 0x18;
        words(d);
        words(ptr_at(d, 4)); /* 0x3C float params per entry */
    }
    words(ptr_at(p, 0xC));
    word_ptr_array(ptr_at(p, 0x10));
}

/// +44 environment collision (ftData_x44_t): 6 s16 then 4 f32.
static void swap_env_coll(ftData_x44_t* e)
{
    if (!OK(e)) {
        return;
    }
    port_swap16_array(e, 6);
    port_swap32_array(&e->unkC, 4);
}

/// +58 IK (ftData_x58_t): byte pairs interleaved with floats.
static void swap_ik(struct ftData_x58_t* k)
{
    if (!OK(k)) {
        return;
    }
    port_swap32(&k->x4);
    port_swap32(&k->xC);
    port_swap32(&k->x18);
    port_swap32((u8*) k + 0x14);
    port_swap32((u8*) k + 0x20);
    port_swap32((u8*) k + 0x28);
    port_swap32((u8*) k + 0x2C);
    port_swap32((u8*) k + 0x30);
}

/// +4C SFX table: words plus three s32 tables { s32 count; s32* ids }.
static void swap_sfx(void* p)
{
    static const size_t tables[] = { 0x00, 0x1C, 0x20 };
    size_t i;
    if (!OK(p)) {
        return;
    }
    words(p);
    for (i = 0; i < 3; i++) {
        void* t = ptr_at(p, tables[i]);
        if (OK(t)) {
            words(t);
            words(ptr_at(t, 4));
        }
    }
}

/// Special attribute structs (ftData +04) that are not all 32-bit words:
/// reflector/cape/bat `behavior` bytes, sword trail colors, Game & Watch
/// colors, Kirby's s16. Their generated swaps convert (or claim) those
/// fields before the extent word pass. Other fighters' attributes are all
/// f32/s32 in the data.
static const struct {
    const char* symbol;
    void (*swap)(void*);
} typed_attrs[] = {
    { "ftDataFox", port_swap_ftFox_DatAttrs },
    { "ftDataFalco", port_swap_ftFox_DatAttrs },
    { "ftDataMario", port_swap_ftMario_DatAttrs },
    { "ftDataDrmario", port_swap_ftMario_DatAttrs },
    { "ftDataMewtwo", port_swap_ftMewtwoAttributes },
    { "ftDataNess", port_swap_ftNessAttributes },
    { "ftDataZelda", port_swap_ftZelda_DatAttrs },
    { "ftDataKirby", port_swap_ftKb_DatAttrs },
    { "ftDataLink", port_swap_ftLk_DatAttrs },
    { "ftDataClink", port_swap_ftLk_DatAttrs },
    { "ftDataGamewatch", port_swap_ftGameWatchAttributes },
    { "ftDataMars", port_swap_MarsAttributes },
    { "ftDataEmblem", port_swap_MarsAttributes },
};

static void swap_ftData(const char* symbol, void* addr)
{
    ftData* d = addr;
    size_t i, n;

    port_swap_ftCo_DatAttrs(d->x0);
    if (OK(d->ext_attr)) {
        for (i = 0; i < sizeof(typed_attrs) / sizeof(typed_attrs[0]); i++) {
            if (strcmp(symbol, typed_attrs[i].symbol) == 0) {
                typed_attrs[i].swap(d->ext_attr);
                break;
            }
        }
    }
    words(d->ext_attr);
    swap_model_lookup(d->x8);
    swap_action_table(d->xC);
    /* +10 / +18: u8 pairs, nothing to swap. */
    swap_action_table(d->x14);
    swap_part_anims(d->x1C);
    if (OK(d->x20)) { /* shield pose: { HSD_Joint* } (extent 4) */
        words(d->x20);
        port_walk_Joint(ptr_at(d->x20, 0));
    }
    words(d->x24); /* idle action chances */
    words(d->x28);
    swap_physics(d->x2C);
    if (OK(d->x30)) { /* hurtboxes: { s32 n; 0x28-byte all-word rows } */
        words(d->x30);
        words(d->x30->inits);
    }
    words(d->x34); /* center bubble */
    words(d->x38); /* coin collision spheres */
    words(d->x3C); /* camera box */
    words(d->x40); /* item pickup ranges */
    swap_env_coll(d->x44);
    if (OK(d->x48_items)) {
        /* Mostly Article* (0x18: ItemAttr* (0x84), ...), but some slots
         * hold other data: Fox [4] is a small s32 table, Samus [4] a model
         * set Kirby uses for the copied hat. Only article-shaped entries get
         * the article walker; the rest get the word pass (pointer words are
         * untouched), their deeper structure is not converted yet. */
        n = port_extent(d->x48_items) / 4;
        for (i = 0; i < n; i++) {
            Article* a = d->x48_items[i];
            if (!OK(a)) {
                continue;
            }
            if (port_extent(a) == sizeof(Article) &&
                OK(a->x0_common_attr) &&
                port_extent(a->x0_common_attr) == 0x84)
            {
                port_swap_article(a);
            } else {
                words(a);
            }
        }
    }
    swap_sfx(d->x4C_sfx);
    words(d->x50); /* jostle box */
    words(d->x54); /* bone ids */
    swap_ik(d->x58);
    port_walk_Joint(d->x5C); /* metal model */
}

/* ---- ftLoadCommonData (PlCo.dat) ------------------------------------- */

/// { script*; u8 priority; u8 slot; u8 pad[2] } color animation rows.
void port_swap_color_anims(void* p)
{
    size_t i, n;
    if (!OK(p)) {
        return;
    }
    n = port_extent(p) / 8;
    for (i = 0; i < n; i++) {
        void* script = ptr_at(p, i * 8);
        if (OK(script)) {
            port_swap_script(script);
        }
    }
}

/// { Vec2* entries; s32 count } shake tables.
static void swap_counted_words(void* p)
{
    if (OK(p)) {
        words(p);
        words(ptr_at(p, 0));
    }
}

/// The 23 pointers copied out by Fighter_LoadCommonData (fighter.c),
/// classified against the data (HSDLib SBM_ftLoadCommonData names a few).
static void swap_ftLoadCommonData(void* addr)
{
    size_t i, n;
    void* cpu;

    port_swap_ftCommonData(ptr_at(addr, 0x00)); /* p_ftCommonData */
    words(ptr_at(addr, 0x04));                  /* float tables */
    words(ptr_at(addr, 0x08));
    words(ptr_at(addr, 0x0C));
    word_ptr_array(ptr_at(addr, 0x10)); /* ftPartsTable: {u8*, u8*, u32} */
    word_ptr_array(ptr_at(addr, 0x14)); /* {u8[4]* entries; s32 count} */
    port_swap_color_anims(ptr_at(addr, 0x18));
    port_swap_color_anims(ptr_at(addr, 0x1C));
    if (OK(ptr_at(addr, 0x20))) { /* respawn platform {joint, animjoint} */
        void* rp = ptr_at(addr, 0x20);
        port_walk_Joint(ptr_at(rp, 0));
        port_walk_AnimJoint(ptr_at(rp, 4));
    }
    { /* +24: three {Vec2* v; s32 n} tables in one object */
        void* t = ptr_at(addr, 0x24);
        if (OK(t)) {
            words(t);
            for (i = 0; i < port_extent(t) / 8; i++) {
                words(ptr_at(t, i * 8));
            }
        }
    }
    swap_counted_words(ptr_at(addr, 0x28)); /* grab-mash shake */
    swap_counted_words(ptr_at(addr, 0x2C)); /* smash-charge shake */
    for (i = 0x30; i <= 0x3C; i += 4) {
        words(ptr_at(addr, i));
    }
    port_walk_Joint(ptr_at(addr, 0x40)); /* trophy stand model */
    /* +44..+4C: RGBA colors, bytes. */
    port_walk_Joint(ptr_at(addr, 0x50)); /* cube model */
    words(ptr_at(addr, 0x54));           /* crowd config */

    cpu = ptr_at(addr, 0x58); /* CPU tables (Fighter_804D64FC_t) */
    if (OK(cpu)) {
        /* +00: per-character CPU command byte scripts: bytes. */
        for (i = 0x04; i <= 0x1C; i += 4) {
            word_ptr_array(ptr_at(cpu, i));
        }
        words(ptr_at(cpu, 0x20));
        words(ptr_at(cpu, 0x24));
    }
    (void) n;
}

/* ---- roots ------------------------------------------------------------ */

int port_swap_fighter_public(const char* symbol, void* addr)
{
    if (strcmp(symbol, "plLoadCommonData") == 0) {
        /* PdPm.dat: { pl_804D6470_t* } with 0x184 bytes of f32/s32. */
        words(ptr_at(addr, 0));
        return 1;
    }
    if (strcmp(symbol, "ftLoadCommonData") == 0) {
        swap_ftLoadCommonData(addr);
        return 1;
    }
    if (strncmp(symbol, "ftData", 6) == 0) {
        swap_ftData(symbol, addr);
        return 1;
    }
    return 0;
}
