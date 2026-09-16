/// VI (video interface) for the headless port.
///
/// On hardware the VI interrupt fires every field and the SDK's handler
/// runs the pre- and post-retrace callbacks; sysdolphin's XFB state
/// machine (HSD_VIPreRetraceCB / HSD_VIPostRetraceCB in
/// src/sysdolphin/baselib/video.c) only advances inside those callbacks.
/// The OS layer's virtual clock raises port_vi_interrupt() every 59.94 Hz
/// field; VIWaitForRetrace() just advances time until that happens.

#include <dolphin/gx.h>
#include <dolphin/vi.h>

#include <stdlib.h>

#include <sysdolphin/baselib/video.h>

#include <port/port.h>

static u32 retrace_count;
static VIRetraceCallback pre_cb;
static VIRetraceCallback post_cb;
static void* next_fb;
static void* current_fb;
static BOOL black = 1;
static GXRenderModeObj rmode;

void VIInit(void) {}

void VIConfigure(GXRenderModeObj* rm)
{
    if (rm) {
        rmode = *rm;
    }
}

void VIFlush(void) {}

void VISetBlack(BOOL b)
{
    black = b;
}

void VISetNextFrameBuffer(void* fb)
{
    next_fb = fb;
}

void* VIGetNextFrameBuffer(void)
{
    return next_fb;
}

void* VIGetCurrentFrameBuffer(void)
{
    return current_fb;
}

u32 VIGetRetraceCount(void)
{
    return retrace_count;
}

u32 VIGetNextField(void)
{
    return retrace_count & 1;
}

u32 VIGetTvFormat(void)
{
    return VI_NTSC;
}

u32 VIGetDTVStatus(void)
{
    return 0;
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb)
{
    VIRetraceCallback old = pre_cb;
    pre_cb = cb;
    return old;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb)
{
    VIRetraceCallback old = post_cb;
    post_cb = cb;
    return old;
}

/// Headless run control: MELEE_PORT_FRAMES=N exits cleanly after N
/// retraces; a heartbeat line is logged every 600 retraces (10 s of game
/// time).
static void check_frame_limit(void)
{
    static long limit = -2;
    if (limit == -2) {
        const char* s = getenv("MELEE_PORT_FRAMES");
        limit = s ? strtol(s, NULL, 10) : -1;
    }
    if (retrace_count % 600 == 0) {
        port_log("retrace %u", retrace_count);
    }
    if (limit >= 0 && retrace_count >= (u32) limit) {
        port_log("reached MELEE_PORT_FRAMES=%ld, exiting", limit);
        {
            extern u32 gm_801A4BA8(void);
            port_log("scene logic frames: %u", gm_801A4BA8());
        }
        port_log("xfb status: %d %d %d, nb_xfb=%d, efb=%d, drawdone.waiting=%d",
                 HSD_VIData.xfb[0].status, HSD_VIData.xfb[1].status,
                 HSD_VIData.xfb[2].status, HSD_VIData.nb_xfb,
                 HSD_VIData.efb.status, HSD_VIData.drawdone.waiting);
        if (getenv("MELEE_PORT_ABORT_AT_LIMIT")) {
            abort(); /* crash handler prints where the game was */
        }
        exit(0);
    }
}

void VIWaitForRetrace(void)
{
    port_wait_retrace();
}

void port_vi_interrupt(void)
{
    retrace_count++;
    check_frame_limit();
    if (pre_cb) {
        pre_cb(retrace_count);
    }
    current_fb = next_fb;
    if (post_cb) {
        post_cb(retrace_count);
    }
}
