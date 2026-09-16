/// Item DAT data: fighter articles (ftData +48), and later ItCo.dat
/// itPublicData and stage itemdata.
///
/// Layouts: src/melee/it/types.h (Article, ItemAttr, ItemStateDesc,
/// ItemModelDesc, ItHurtBoneList), cross-checked against HSDLib's
/// SBM_Article / SBM_ItemCommonAttr / SBM_ItemState / SBM_ItemModel.

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
        port_swap_ItemModelDesc(a->x10_modelDesc);
        port_walk_Joint(a->x10_modelDesc->x0_joint);
    }
    if (OK(a->x14_dynamics)) { /* { s32 n; BoneDynamicsDesc* } */
        ItemDynamics* d = a->x14_dynamics;
        words(d);
        for (i = 0; OK(d->dyn_descs) && i < (size_t) d->count; i++) {
            void* desc = (u8*) d->dyn_descs + i * 0x18;
            words(desc);
            words(*(void**) ((u8*) desc + 4)); /* 0x3C float params */
        }
    }
}
