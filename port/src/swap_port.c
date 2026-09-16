/// Typed byte-swapping of HSD object graphs loaded from DAT archives.
///
/// Entry point: port_swap_public(), called when game code looks up a
/// public symbol in an archive. HSD symbol names end in a suffix that
/// names the root type (`_joint`, `_animjoint`, `_camera`, ...); from the
/// root the walker follows every typed pointer and converts each struct's
/// scalar fields with the generated port_swap_<Type>() functions
/// (swap_gen.c). Swaps are idempotent (see endian_port.c), so shared
/// subgraphs and repeated lookups are harmless.
///
/// Deliberately left big-endian (decoded later by their consumers):
///   - display lists, vertex arrays, texture and palette data (the GX
///     renderer reads them as the GPU would)
///   - FObj keyframe bytes (`ad`): fobj.c already decodes them byte-wise

#include <string.h>

#include <sysdolphin/baselib/aobj.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/dobj.h>
#include <sysdolphin/baselib/fog.h>
#include <sysdolphin/baselib/jobj.h>
#include <sysdolphin/baselib/lobj.h>
#include <sysdolphin/baselib/mobj.h>
#include <sysdolphin/baselib/pobj.h>
#include <sysdolphin/baselib/psstructs.h>
#include <sysdolphin/baselib/robj.h>
#include <sysdolphin/baselib/spline.h>
#include <sysdolphin/baselib/tobj.h>
#include <sysdolphin/baselib/wobj.h>
#include <melee/sc/types.h>

#include <port/endian.h>
#include <port/port.h>
#include <port/swap.h>

/// Only walk into memory that belongs to a loaded archive.
#define OK(p) ((p) != NULL && port_in_archive(p))

/* ---- animation ---- */

void port_walk_AObjDesc(HSD_AObjDesc* a)
{
    HSD_FObjDesc* f;
    if (!OK(a)) {
        return;
    }
    port_swap_HSD_AObjDesc(a);
    for (f = a->fobjdesc; OK(f); f = f->next) {
        port_swap_HSD_FObjDesc(f);
    }
}

static void walk_WObjAnim(HSD_WObjAnim* w)
{
    HSD_RObjAnimJoint* r;
    if (!OK(w)) {
        return;
    }
    port_walk_AObjDesc(w->aobjdesc);
    for (r = w->robjanim; OK(r); r = r->next) {
        port_walk_AObjDesc(r->aobjdesc);
    }
}

void port_walk_AnimJoint(HSD_AnimJoint* j)
{
    HSD_RObjAnimJoint* r;
    for (; OK(j); j = j->next) {
        port_swap_HSD_AnimJoint(j);
        port_walk_AObjDesc(j->aobjdesc);
        for (r = j->robj_anim; OK(r); r = r->next) {
            port_walk_AObjDesc(r->aobjdesc);
        }
        port_walk_AnimJoint(j->child);
    }
}

static void walk_TlutDesc(HSD_TlutDesc* t)
{
    if (OK(t)) {
        port_swap_HSD_TlutDesc(t); /* palette bytes stay BE */
    }
}

void port_walk_MatAnimJoint(HSD_MatAnimJoint* j)
{
    for (; OK(j); j = j->next) {
        HSD_MatAnim* m;
        for (m = j->matanim; OK(m); m = m->next) {
            HSD_TexAnim* t;
            port_walk_AObjDesc(m->aobjdesc);
            for (t = m->texanim; OK(t); t = t->next) {
                int i;
                port_swap_HSD_TexAnim(t);
                port_walk_AObjDesc(t->aobjdesc);
                for (i = 0; OK(t->imagetbl) && i < t->n_imagetbl; i++) {
                    if (OK(t->imagetbl[i])) {
                        port_swap_HSD_ImageDesc(t->imagetbl[i]);
                    }
                }
                for (i = 0; OK(t->tluttbl) && i < t->n_tluttbl; i++) {
                    walk_TlutDesc(t->tluttbl[i]);
                }
            }
            if (OK(m->renderanim)) {
                HSD_ChanAnim* c;
                HSD_TevRegAnim* r;
                for (c = m->renderanim->chananim; OK(c); c = c->next) {
                    port_walk_AObjDesc(c->aobjdesc);
                }
                for (r = m->renderanim->reganim; OK(r); r = r->next) {
                    port_walk_AObjDesc(r->aobjdesc);
                }
            }
        }
        port_walk_MatAnimJoint(j->child);
    }
}

void port_walk_ShapeAnimJoint(HSD_ShapeAnimJoint* j)
{
    for (; OK(j); j = j->next) {
        HSD_ShapeAnimDObj* d;
        for (d = j->shapeanimdobj; OK(d); d = d->next) {
            HSD_ShapeAnim* s;
            for (s = d->shapeanim; OK(s); s = s->next) {
                port_walk_AObjDesc(s->aobjdesc);
            }
        }
        port_walk_ShapeAnimJoint(j->child);
    }
}

/* ---- models ---- */

static void walk_RObjDesc(HSD_RObjDesc* r)
{
    for (; OK(r); r = r->next) {
        port_swap_HSD_RObjDesc(r);
        /* u is a u32/f32 or a pointer; if a pointer, this is a no-op. */
        port_swap32(&r->u);
        /* @todo expression / bytecode / IK-hint payloads */
    }
}

static void walk_VtxDescList(HSD_VtxDescList* v)
{
    /* Array terminated by attr == GX_VA_NULL (0xFF). The vertex data
     * itself stays big-endian for the renderer. */
    for (; OK(v); v++) {
        port_swap_HSD_VtxDescList(v);
        if (v->attr == GX_VA_NULL) {
            break;
        }
    }
}

static void walk_Joint(HSD_Joint* j);

static void walk_PObjDesc(HSD_PObjDesc* p)
{
    for (; OK(p); p = p->next) {
        port_swap_HSD_PObjDesc(p);
        walk_VtxDescList(p->verts);
        switch (p->flags & 0x3000) {
        case POBJ_SKIN:
            walk_Joint(p->u.joint);
            break;
        case POBJ_SHAPEANIM:
            if (OK(p->u.shape_set)) {
                HSD_ShapeSetDesc* s = p->u.shape_set;
                port_swap_HSD_ShapeSetDesc(s);
                walk_VtxDescList(s->vertex_desc);
                walk_VtxDescList(s->normal_desc);
            }
            break;
        case POBJ_ENVELOPE:
            if (OK(p->u.envelope_p)) {
                HSD_EnvelopeDesc** list;
                for (list = p->u.envelope_p; OK(list) && *list; list++) {
                    HSD_EnvelopeDesc* e;
                    for (e = *list; OK(e) && e->joint; e++) {
                        port_swap_HSD_EnvelopeDesc(e);
                    }
                }
            }
            break;
        }
    }
}

static void walk_TObjDesc(HSD_TObjDesc* t)
{
    for (; OK(t); t = t->next) {
        port_swap_HSD_TObjDesc(t);
        if (OK(t->imagedesc)) {
            port_swap_HSD_ImageDesc(t->imagedesc);
        }
        walk_TlutDesc(t->tlutdesc);
        if (OK(t->lod)) {
            port_swap_HSD_TexLODDesc(t->lod);
        }
        if (OK(t->tev)) {
            port_swap_HSD_TObjTevDesc(t->tev);
        }
    }
}

void port_walk_MObjDesc(HSD_MObjDesc* m)
{
    if (!OK(m)) {
        return;
    }
    port_swap_HSD_MObjDesc(m);
    walk_TObjDesc(m->texdesc);
    if (OK(m->mat)) {
        port_swap_HSD_Material(m->mat);
    }
    if (OK(m->pedesc)) {
        port_swap_HSD_PEDesc(m->pedesc);
    }
}

static void walk_DObjDesc(HSD_DObjDesc* d)
{
    for (; OK(d); d = d->next) {
        port_swap_HSD_DObjDesc(d);
        port_walk_MObjDesc(d->mobjdesc);
        walk_PObjDesc(d->pobjdesc);
    }
}

void port_walk_Spline(HSD_Spline* s)
{
    if (!OK(s)) {
        return;
    }
    port_swap_HSD_Spline(s);
    if (OK(s->cv)) {
        port_swap32_array(s->cv, (size_t) s->numcv * 3);
    }
    if (OK(s->segLength)) {
        port_swap32_array(s->segLength, (size_t) s->numcv);
    }
    if (OK(s->segPoly)) {
        port_swap32_array(s->segPoly, (size_t) s->numcv * 5);
    }
}

static void walk_Joint(HSD_Joint* j)
{
    for (; OK(j); j = j->next) {
        port_swap_HSD_Joint(j);
        if (j->flags & JOBJ_SPLINE) {
            port_walk_Spline(j->u.spline);
        } else if (!(j->flags & JOBJ_PTCL)) {
            walk_DObjDesc(j->u.dobjdesc);
        }
        if (OK(j->mtx)) {
            port_swap32_array(j->mtx, 12);
        }
        walk_RObjDesc(j->robjdesc);
        if (!(j->flags & JOBJ_INSTANCE)) {
            walk_Joint(j->child);
        }
    }
}

void port_walk_Joint(HSD_Joint* j)
{
    walk_Joint(j);
}

/* ---- world / camera / lights / fog ---- */

static void walk_WObjDesc(HSD_WObjDesc* w)
{
    if (OK(w)) {
        port_swap_HSD_WObjDesc(w);
        walk_RObjDesc(w->robjdesc);
    }
}

void port_walk_CObjDesc(HSD_CObjDesc* c)
{
    if (!OK(c)) {
        return;
    }
    port_swap_HSD_CameraDescCommon(&c->common);
    if (c->common.projection_type == PROJ_PERSPECTIVE) {
        port_swap_HSD_CameraDescPerspective(&c->perspective);
    } else {
        port_swap_HSD_CameraDescFrustum(&c->frustum);
    }
    walk_WObjDesc(c->common.eyepos);
    walk_WObjDesc(c->common.interest);
    if (OK(c->common.up_vector)) {
        port_swap32_array(c->common.up_vector, 3);
    }
}

void port_walk_CameraAnim(HSD_CameraAnim* a)
{
    if (OK(a)) {
        port_walk_AObjDesc(a->aobjdesc);
        walk_WObjAnim(a->eye_anim);
        walk_WObjAnim(a->interest_anim);
    }
}

void port_walk_LightDesc(HSD_LightDesc* l)
{
    for (; OK(l); l = l->next) {
        port_swap_HSD_LightDesc(l);
        walk_WObjDesc(l->position);
        walk_WObjDesc(l->interest);
        switch (l->flags & LOBJ_TYPE_MASK) {
        case LOBJ_POINT:
            if (l->attnflags & LOBJ_LIGHT_ATTN) {
                if (OK(l->u.attn)) {
                    port_swap_HSD_LightAttn(l->u.attn);
                }
            } else if (OK(l->u.point)) {
                port_swap_HSD_LightPointDesc(l->u.point);
            }
            break;
        case LOBJ_SPOT:
            if (l->attnflags != 0) {
                if (OK(l->u.attn)) {
                    port_swap_HSD_LightAttn(l->u.attn);
                }
            } else if (OK(l->u.spot)) {
                port_swap_HSD_LightSpotDesc(l->u.spot);
            }
            break;
        default:
            if ((l->flags & LOBJ_SPECULAR) && OK(l->u.shininess)) {
                port_swap32(l->u.shininess);
            }
            break;
        }
    }
}

void port_walk_LightAnim(HSD_LightAnim* a)
{
    for (; OK(a); a = a->next) {
        port_walk_AObjDesc(a->aobjdesc);
        walk_WObjAnim(a->position_anim);
        walk_WObjAnim(a->interest_anim);
    }
}

void port_walk_FogDesc(HSD_FogDesc* f)
{
    if (OK(f)) {
        port_swap_HSD_FogDesc(f);
        if (OK(f->fogadjdesc)) {
            port_swap_HSD_FogAdjDesc(f->fogadjdesc);
        }
    }
}

/* ---- scene-level containers (src/melee/sc/types.h) ---- */

#define EACH(T, arr, v)                                                       \
    for (T* const* v##_it = (T* const*) (arr); OK(v##_it) && *v##_it;        \
         v##_it++)                                                            \
        for (T* v = *v##_it; v; v = NULL)

void port_walk_StaticModelDesc(StaticModelDesc* m)
{
    if (OK(m)) {
        walk_Joint(m->joint);
        port_walk_AnimJoint(m->animjoint);
        port_walk_MatAnimJoint(m->matanim_joint);
        port_walk_ShapeAnimJoint(m->shapeanim_joint);
    }
}

void port_walk_DynamicModelDesc(DynamicModelDesc* m)
{
    if (!OK(m)) {
        return;
    }
    walk_Joint(m->joint);
    EACH(HSD_AnimJoint, m->anims, a) { port_walk_AnimJoint(a); }
    EACH(HSD_MatAnimJoint, m->matanims, a) { port_walk_MatAnimJoint(a); }
    EACH(HSD_ShapeAnimJoint, m->shapeanims, a)
    {
        port_walk_ShapeAnimJoint(a);
    }
}

void port_walk_LightLists(LightList** lists)
{
    EACH(LightList, lists, l)
    {
        port_walk_LightDesc(l->desc);
        EACH(HSD_LightAnim, l->anims, a) { port_walk_LightAnim(a); }
    }
}

void port_walk_SceneDesc(SceneDesc* s)
{
    if (!OK(s)) {
        return;
    }
    EACH(DynamicModelDesc, s->models, m) { port_walk_DynamicModelDesc(m); }
    if (OK(s->cameras)) {
        port_walk_CObjDesc(s->cameras->desc);
        EACH(HSD_CameraAnim, s->cameras->anims, a) { port_walk_CameraAnim(a); }
    }
    EACH(LightList, s->lights, l)
    {
        port_walk_LightDesc(l->desc);
        EACH(HSD_LightAnim, l->anims, a) { port_walk_LightAnim(a); }
    }
    if (OK(s->fogs)) {
        port_walk_FogDesc(s->fogs->desc);
        EACH(HSD_CameraAnim, s->fogs->anims, a) { port_walk_CameraAnim(a); }
    }
}

/* ---- particle banks (sysdolphin/baselib/particle.c, psstructs.h) ---- */

static void swap_PSCmdList(HSD_PSCmdList* cl)
{
    /* type, texGroup, genLife, life */
    port_swap16_array(cl, 4);
    /* kind, then grav..param3: 12 words; cmdList[] bytes stay as-is
     * (particle.c decodes them byte-wise). */
    port_swap32_array(&cl->kind, 12);
}

void port_swap_ps_banks(void* cmdBank, void* texBank, int* formBank)
{
    s32* cmd = cmdBank;
    s32* tex = texBank;
    s32 i;

    /* Command bank: u16 version, then either
     *   v0:        [1]=count, [2..] offsets
     *   v0x40-43:  [1]=num,   [2]=count, [3..] offsets
     * The offsets are bank-relative until psInitDataBankLocate runs. */
    if (OK(cmd)) {
        u32 version, count, first;
        port_swap16(cmd);
        port_swap16((u8*) cmd + 2);
        version = *(u16*) cmd;
        if (version == 0) {
            port_swap32(&cmd[1]);
            count = (u32) cmd[1];
            first = 2;
        } else if (version >= 0x40 && version < 0x44) {
            port_swap32_array(&cmd[1], 2);
            count = (u32) cmd[2];
            first = 3;
        } else {
            port_log("port_swap_ps_banks: unknown cmd bank version 0x%x",
                     version);
            count = 0;
            first = 0;
        }
        port_swap32_array(&cmd[first], count);
        for (i = 0; (u32) i < count; i++) {
            if (cmd[first + i] != 0) {
                swap_PSCmdList(
                    (HSD_PSCmdList*) ((u8*) cmd + (u32) cmd[first + i]));
            }
        }
    }

    /* Texture bank: [0]=num_groups, [1..num_groups] group offsets. */
    if (OK(tex)) {
        s32 num_groups;
        port_swap32(&tex[0]);
        num_groups = tex[0];
        port_swap32_array(&tex[1], (size_t) num_groups);
        for (i = 1; i <= num_groups; i++) {
            HSD_PSTexGroup* tg;
            u32 n;
            if (tex[i] == 0) {
                continue;
            }
            tg = (HSD_PSTexGroup*) ((u8*) tex + (u32) tex[i]);
            port_swap32_array(tg, 5); /* num, fmt, tlutfmt, width, height */
            port_swap16(&tg->palnum);
            port_swap16(&tg->palflag);
            /* Entry count, mirroring psInitDataBankLocate's palette rules. */
            n = tg->num;
            if (tg->fmt == 8 || tg->fmt == 9 || tg->fmt == 10) {
                if (tg->palflag & 1) {
                    n += 1;
                } else if (tg->palnum != 0) {
                    n += tg->palnum;
                } else {
                    n = tg->num * 2;
                }
            }
            port_swap32_array(tg->texTable, n); /* image data stays BE */
        }

        /* Form bank: [1..num_groups] offsets to {num, formTable[num]}. */
        if (formBank != NULL && OK(formBank)) {
            for (i = 1; i <= num_groups; i++) {
                HSD_PSFormGroup* fg;
                port_swap32(&formBank[i]);
                if (formBank[i] == 0) {
                    continue;
                }
                fg = (HSD_PSFormGroup*) ((u8*) formBank + (u32) formBank[i]);
                port_swap32(&fg->num);
                port_swap32_array(fg->formTable, fg->num);
            }
        }
    }
}

/* ---- symbol-name dispatch ---- */

static int ends_with(const char* s, const char* suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

void port_swap_public(const char* symbol, void* addr)
{
    if (!OK(addr)) {
        return;
    }
    /* Order matters: longer suffixes first. */
    if (ends_with(symbol, "_shapeanim_joint")) {
        port_walk_ShapeAnimJoint(addr);
    } else if (ends_with(symbol, "_matanim_joint")) {
        port_walk_MatAnimJoint(addr);
    } else if (ends_with(symbol, "_animjoint")) {
        port_walk_AnimJoint(addr);
    } else if (ends_with(symbol, "_joint")) {
        walk_Joint(addr);
    } else if (ends_with(symbol, "_scene_data") ||
               ends_with(symbol, "_scene_models")) {
        port_walk_SceneDesc(addr);
    } else if (ends_with(symbol, "_camera")) {
        port_walk_CObjDesc(addr);
    } else if (ends_with(symbol, "_lights")) {
        port_walk_LightLists(addr);
    } else if (ends_with(symbol, "_fog")) {
        port_walk_FogDesc(addr);
    } else if (!port_swap_game_public(symbol, addr)) {
        port_log("port_swap_public: no type known for symbol '%s'", symbol);
    }
}
