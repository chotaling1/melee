#ifndef PORT_SWAP_H
#define PORT_SWAP_H

/// Typed byte-swap walkers for DAT data. See port/src/swap_port.c.

#include <sysdolphin/baselib/forward.h>
#include <melee/sc/forward.h>

/* Generated per-struct field swaps (port/src/swap_gen.c). */
void port_swap_HSD_WObjDesc(void* p);
void port_swap_HSD_CameraDescCommon(void* p);
void port_swap_HSD_CameraDescFrustum(void* p);
void port_swap_HSD_CameraDescPerspective(void* p);
void port_swap_HSD_Joint(void* p);
void port_swap_HSD_DObjDesc(void* p);
void port_swap_HSD_MObjDesc(void* p);
void port_swap_HSD_Material(void* p);
void port_swap_HSD_PEDesc(void* p);
void port_swap_HSD_TObjDesc(void* p);
void port_swap_HSD_ImageDesc(void* p);
void port_swap_HSD_TlutDesc(void* p);
void port_swap_HSD_TObjTevDesc(void* p);
void port_swap_HSD_PObjDesc(void* p);
void port_swap_HSD_ShapeSetDesc(void* p);
void port_swap_HSD_ShapeAnim(void* p);
void port_swap_HSD_ShapeAnimJoint(void* p);
void port_swap_HSD_ShapeAnimDObj(void* p);
void port_swap_HSD_EnvelopeDesc(void* p);
void port_swap_HSD_VtxDescList(void* p);
void port_swap_HSD_AObjDesc(void* p);
void port_swap_HSD_FObjDesc(void* p);
void port_swap_HSD_AnimJoint(void* p);
void port_swap_HSD_MatAnimJoint(void* p);
void port_swap_HSD_MatAnim(void* p);
void port_swap_HSD_TexAnim(void* p);
void port_swap_HSD_LightDesc(void* p);
void port_swap_HSD_LightAttn(void* p);
void port_swap_HSD_FogDesc(void* p);
void port_swap_HSD_FogAdjDesc(void* p);
void port_swap_HSD_RObjDesc(void* p);
void port_swap_HSD_RObjAnimJoint(void* p);
void port_swap_HSD_CameraAnim(void* p);
void port_swap_HSD_WObjAnim(void* p);
void port_swap_HSD_LightPointDesc(void* p);
void port_swap_HSD_LightSpotDesc(void* p);
void port_swap_HSD_TexLODDesc(void* p);
void port_swap_HSD_Spline(void* p);

/* Generated: stages. */
void port_swap_GroundParam(void* p);
void port_swap_StageParam(void* p);
void port_swap_MapCollData(void* p);
void port_swap_MapLine(void* p);
void port_swap_MapJoint(void* p);
void port_swap_UnkStageDat(void* p);
void port_swap_UnkStageDat_x8_t(void* p);
void port_swap_GroundShadowEntry(void* p);
void port_swap_GrJoint(void* p);

/* Generated: fighters, items. */
void port_swap_ftCo_DatAttrs(void* p);
void port_swap_ftCommonData(void* p);
void port_swap_ftFox_DatAttrs(void* p);
void port_swap_ftMario_DatAttrs(void* p);
void port_swap_ftMewtwoAttributes(void* p);
void port_swap_ftNessAttributes(void* p);
void port_swap_ftZelda_DatAttrs(void* p);
void port_swap_ftKb_DatAttrs(void* p);
void port_swap_ftLk_DatAttrs(void* p);
void port_swap_ftGameWatchAttributes(void* p);
void port_swap_MarsAttributes(void* p);
void port_swap_itLeadeadAttributes(void* p);
void port_swap_itOctarockAttributes(void* p);
void port_swap_TrophyData(void* p);
void port_swap_ToyNameData(void* p);
void port_swap_TyDspEntry(void* p);
void port_swap_ItemAttr(void* p);
void port_swap_ItemCommonData(void* p);
void port_swap_ItemModelDesc(void* p);

/* Fighters (port/src/swap_fighter.c) and items (port/src/swap_item.c). */
void port_swap_script(void* p);
void port_swap_article(void* a);
void port_swap_color_anims(void* p);
void port_swap_itPublicData(void* addr);
int port_swap_fighter_public(const char* symbol, void* addr);
int port_swap_misc_public(const char* symbol, void* addr); /* swap_misc.c */

/* Graph walkers (port/src/swap_port.c). */
void port_walk_Joint(HSD_Joint* j);
void port_walk_MObjDesc(struct _HSD_MObjDesc* m);
void port_walk_Spline(struct HSD_Spline* s);
void port_walk_LightLists(struct LightList** lists);
void port_walk_AnimJoint(HSD_AnimJoint* j);
void port_walk_MatAnimJoint(HSD_MatAnimJoint* j);
void port_walk_ShapeAnimJoint(HSD_ShapeAnimJoint* j);
void port_walk_AObjDesc(HSD_AObjDesc* a);
void port_walk_CObjDesc(HSD_CObjDesc* c);
void port_walk_CameraAnim(HSD_CameraAnim* a);
void port_walk_LightDesc(HSD_LightDesc* l);
void port_walk_LightAnim(HSD_LightAnim* a);
void port_walk_FogDesc(HSD_FogDesc* f);
void port_walk_SceneDesc(SceneDesc* s);
void port_walk_StaticModelDesc(StaticModelDesc* m);
void port_walk_DynamicModelDesc(DynamicModelDesc* m);
void port_walk_DynamicModelDescs(DynamicModelDesc** models);

/// Particle banks (sysdolphin/baselib/particle.c): command bank, texture
/// bank and optional form bank, before psInitDataBankLocate relocates
/// their internal offsets. Arguments are the untyped bank pointers.
void port_swap_ps_banks(void* cmdBank, void* texBank, int* formBank);

/// Swap the object graph rooted at a public archive symbol, choosing the
/// type from the symbol name.
void port_swap_public(const char* symbol, void* addr);

/// Game-specific (non-sysdolphin) root types, by symbol name. Returns 1 if
/// the symbol was recognised. Implemented in port/src/swap_game.c.
int port_swap_game_public(const char* symbol, void* addr);

#endif
