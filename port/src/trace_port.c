/// Headless simulation trace.
///
///     MELEE_PORT_TRACE=N
///
/// prints every fighter's kind, motion state, position, velocity, damage and
/// stocks every N logic frames (called after HSD_GObj_RunProcs in the scene
/// loop, src/melee/gm/gmscene.c). With no window yet, this is how to check
/// that a match is actually simulating.

#include <stdlib.h>

#include <melee/ft/types.h>
#include <melee/gm/gmscene.h>
#include <melee/pl/player.h>

#include <port/port.h>

void port_trace_frame(void)
{
    static long every = -1;
    u32 frame;
    int slot;

    if (every < 0) {
        const char* s = getenv("MELEE_PORT_TRACE");
        every = s != NULL ? atol(s) : 0;
    }
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
        port_log("f%u p%d kind %d motion %d %s pos (%.3f, %.3f) vel (%.3f, "
                 "%.3f) dmg %.1f stocks %d",
                 frame, slot, fp->kind, fp->motion_id,
                 fp->ground_or_air == GA_Air ? "air" : "gnd", fp->cur_pos.x,
                 fp->cur_pos.y, fp->self_vel.x, fp->self_vel.y,
                 fp->dmg.x1830_percent, Player_GetStocks(slot));
    }
}
