/// Headless simulation trace.
///
///     MELEE_PORT_TRACE=N
///
/// prints every fighter's kind, motion state, position, velocity, damage and
/// stocks every N logic frames (called after HSD_GObj_RunProcs in the scene
/// loop, src/melee/gm/gmscene.c). With no window yet, this is how to check
/// that a match is actually simulating. Fighters holding an item are marked
/// "holding item"; spawns of common items and Pokemon (not character
/// projectiles) are logged as they happen.

#include <stdlib.h>

#include <melee/ft/types.h>
#include <melee/gm/gmscene.h>
#include <melee/it/forward.h>
#include <melee/pl/player.h>

#include <port/port.h>

static long trace_every(void)
{
    static long every = -1;
    if (every < 0) {
        const char* s = getenv("MELEE_PORT_TRACE");
        every = s != NULL ? atol(s) : 0;
    }
    return every;
}

void port_trace_item_spawn(int kind, float x, float y)
{
    if (trace_every() <= 0) {
        return;
    }
    if (kind >= It_Kind_Kuriboh && kind < It_PKind_Start) {
        return;
    }
    port_log("f%u item spawn kind %d pos (%.3f, %.3f)", gm_801A4BA8(), kind,
             x, y);
}

void port_trace_frame(void)
{
    long every = trace_every();
    u32 frame;
    int slot;

    if (every <= 0) {
        return;
    }
    frame = gm_801A4BA8();
    if (frame % every != 0) {
        return;
    }
    for (slot = 0; slot < 4; slot++) {
        HSD_GObj* gobj = Player_GetEntity(slot);
        Fighter* fp;
        if (gobj == NULL || gobj->user_data == NULL) {
            continue;
        }
        fp = gobj->user_data;
        if (frame == 0) {
            const ftCo_DatAttrs* a = &fp->co_attrs;
            port_log("p%d kind %d attrs: weight %g gravity %g terminal %g "
                     "fastfall %g walk_max %g dash_init %g jumpsquat %g "
                     "jumps %d",
                     slot, fp->kind, a->weight, a->gravity,
                     a->terminal_velocity, a->fast_fall_velocity,
                     a->walk_max_vel, a->dash_initial_velocity,
                     a->jump_startup_time, a->max_jumps);
        }
        port_log("f%u p%d kind %d motion %d %s pos (%.3f, %.3f) vel (%.3f, "
                 "%.3f) dmg %.1f stocks %d%s",
                 frame, slot, fp->kind, fp->motion_id,
                 fp->ground_or_air == GA_Air ? "air" : "gnd", fp->cur_pos.x,
                 fp->cur_pos.y, fp->self_vel.x, fp->self_vel.y,
                 fp->dmg.x1830_percent, Player_GetStocks(slot),
                 fp->item_gobj != NULL ? " holding item" : "");
    }
}
