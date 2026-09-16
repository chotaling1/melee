/// Item DAT data: fighter articles (ftData +48), and later ItCo.dat
/// itPublicData and stage itemdata.
///
/// Layouts: src/melee/it/types.h (Article, ItemAttr, ItemStateDesc,
/// ItemModelDesc, ItHurtBoneList), cross-checked against HSDLib's
/// SBM_Article / SBM_ItemCommonAttr / SBM_ItemState / SBM_ItemModel.

#include <melee/it/it_3F14.h>
#include <melee/it/types.h>

#include <port/endian.h>
#include <port/port.h>
#include <port/swap.h>

#define OK(p) ((p) != NULL && port_in_archive(p))

static void words(void* p)
{
    if (OK(p)) {
        port_swap32_extent(p);
    }
}

void port_swap_article(void* addr)
{
    Article* a = addr;
    size_t i, n;

    if (!OK(a)) {
        return;
    }
    /* ItemAttr: packed bitfield bytes then f32/s32 (generated). */
    if (OK(a->x0_common_attr)) {
        port_swap_ItemAttr(a->x0_common_attr);
    }
    words(a->x4_specialAttributes);
    if (OK(a->x8_hurtbones)) { /* { s32 n; 0x20-byte all-word rows } */
        words(a->x8_hurtbones);
        words(a->x8_hurtbones->descs);
    }
    if (OK(a->xC_itemStates)) {
        struct ItemStateDesc* s = a->xC_itemStates->x0_itemStateDesc;
        n = port_extent(s) / sizeof(*s);
        for (i = 0; i < n; i++) {
            port_walk_AnimJoint(s[i].x0_anim_joint);
            port_walk_MatAnimJoint(s[i].x4_matanim_joint);
            port_walk_ShapeAnimJoint(s[i].x8_parameters);
            if (OK(s[i].xC_script)) {
                port_swap_script(s[i].xC_script);
            }
        }
    }
    if (OK(a->x10_modelDesc)) {
        ItemModelDesc* m = a->x10_modelDesc;
        port_swap_ItemModelDesc(m);
        port_walk_Joint(m->x0_joint);
        /* Some itPublicData models are 0x18: { ...; s32; f32[8]* }. */
        if (port_extent(m) == 0x18) {
            words(m);
            words(*(void**) ((u8*) m + 0x14));
        }
    }
    if (OK(a->x14_dynamics)) {
        /* { s32 n; BoneDynamicsDesc*; s32 n_hit; hit bubbles* } (the same
         * physics group as ftData +2C) */
        ItemDynamics* d = a->x14_dynamics;
        words(d);
        for (i = 0; OK(d->dyn_descs) && i < (size_t) d->count; i++) {
            void* desc = (u8*) d->dyn_descs + i * 0x18;
            words(desc);
            words(*(void**) ((u8*) desc + 4)); /* 0x3C float params */
        }
        if (port_extent(d) >= 0x10) {
            words(*(void**) ((u8*) d + 0xC));
        }
    }
}


/* ---- itPublicData (ItCo.usd) ----------------------------------------- */

static inline void* ptr_at(void* obj, size_t off)
{
    return *(void**) ((u8*) obj + off);
}

/// Pointers inside the special attributes of the few itPublicData items
/// that have them (everything else there is f32/s32 words). Offsets are
/// from the disc data; names from the decomp where it has them.
static void swap_special_pointers(ItemKind kind, void* sp)
{
    size_t off;

    switch (kind) {
    case It_Kind_Foods: /* { s32 n; { HSD_Joint*; s32; f32; f32 }[n] } */
        for (off = 4; off + 0x10 <= port_extent(sp); off += 0x10) {
            port_walk_Joint(ptr_at(sp, off));
        }
        break;
    case It_Kind_Kinoko:
    case It_Kind_DKinoko: /* KinokoAnim: HSD_AnimJoint* at +8, +C */
        port_walk_AnimJoint(ptr_at(sp, 0x8));
        port_walk_AnimJoint(ptr_at(sp, 0xC));
        break;
    case It_Kind_WStar: /* +24: s32 n; +28: { HSD_AnimJoint*; s32 }[n] */
        for (off = 0x28; off + 8 <= port_extent(sp); off += 8) {
            port_walk_AnimJoint(ptr_at(sp, off));
        }
        break;
    case It_Kind_Kuriboh:
    case It_Kind_Leadead:
    case It_Kind_Octarock:
    case It_Kind_Ottosea: /* +0: 0x14 bytes of s32/f32 */
        words(ptr_at(sp, 0));
        break;
    case It_Kind_Unk4:
        /* +3C: { HSD_Joint*; HSD_AnimJoint*; HSD_MatAnimJoint*; s32[3];
         * f32[5] } rows, 0x2C each */
        for (off = 0x3C; off + 0x2C <= port_extent(sp); off += 0x2C) {
            port_walk_Joint(ptr_at(sp, off));
            port_walk_AnimJoint(ptr_at(sp, off + 4));
            port_walk_MatAnimJoint(ptr_at(sp, off + 8));
        }
        break;
    case It_PKind_Unknown:
    case It_Kind_Unknown_Swarm: /* +24: HSD_Joint* per Unown letter */
        for (off = 0x24; off + 4 <= port_extent(sp); off += 4) {
            port_walk_Joint(ptr_at(sp, off));
        }
        break;
    default:
        break;
    }
}

/// Article* table of `count` entries starting at item kind `first`; NULL
/// slots (fighter articles live in the fighter DATs) are skipped.
static void article_table(Article** t, ItemKind first, size_t count)
{
    size_t i;
    if (!OK(t)) {
        return;
    }
    if (count > port_extent(t) / 4) {
        count = port_extent(t) / 4;
    }
    for (i = 0; i < count; i++) {
        Article* a = t[i];
        if (!OK(a)) {
            continue;
        }
        /* Special attributes with s16/u8 fields: typed swap first, so the
         * extent word pass in port_swap_article skips those words. */
        if (OK(a->x4_specialAttributes)) {
            switch ((ItemKind) (first + i)) {
            case It_Kind_Leadead:
                port_swap_itLeadeadAttributes(a->x4_specialAttributes);
                break;
            case It_Kind_Octarock:
                port_swap_itOctarockAttributes(a->x4_specialAttributes);
                break;
            default:
                break;
            }
        }
        port_swap_article(a);
        if (OK(a->x4_specialAttributes)) {
            swap_special_pointers((ItemKind) (first + i),
                                  a->x4_specialAttributes);
        }
    }
}

/// ItCo.usd "itPublicData" (it_804D6D20_t, src/melee/it/it_3F14.h), read
/// by it_8027870C (iteffect.c): { ItemCommonData*; Article** common
/// items; Article** from It_Kind_Kuriboh; Article** Pokemon;
/// it_804D6D40_t* (s32 + 6 f32); color animation rows }. Table sizes
/// follow Item_80267978's kind ranges.
void port_swap_itPublicData(void* addr)
{
    it_804D6D20_t* d = addr;

    if (OK(d->x0)) {
        port_swap_ItemCommonData(d->x0);
    }
    article_table(d->x4, 0, It_Kind_Kuriboh);
    article_table(d->x8, It_Kind_Kuriboh, It_PKind_Start - It_Kind_Kuriboh);
    article_table(d->xC, It_PKind_Start, It_Kind_Old_Kuri - It_PKind_Start);
    words(d->x10);
    port_swap_color_anims(d->x14);
}
