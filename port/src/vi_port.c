/// VI (video interface) for the headless port.
///
/// On hardware the VI interrupt fires every field and the SDK's handler
/// runs the pre- and post-retrace callbacks; sysdolphin's XFB state
/// machine (HSD_VIPreRetraceCB / HSD_VIPostRetraceCB in
/// src/sysdolphin/baselib/video.c) only advances inside those callbacks,
/// and HSD_VIWaitXFBFlush spins on VIWaitForRetrace() until it does. So
/// VIWaitForRetrace() *is* the retrace here: it advances time, runs
/// alarms, and calls both callbacks.

#include <dolphin/gx.h>
#include <dolphin/vi.h>

#include <port/port.h>

static u32 retrace_count;
static VIRetraceCallback pre_cb;
static VIRetraceCallback post_cb;
static void* next_fb;
static void* current_fb;
static BOOL black = TRUE;
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

void VIWaitForRetrace(void)
{
    retrace_count++;
    port_os_retrace();
    if (pre_cb) {
        pre_cb(retrace_count);
    }
    current_fb = next_fb;
    if (post_cb) {
        post_cb(retrace_count);
    }
}
