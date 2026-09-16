/// GX pieces that need real behaviour even with no renderer.
///
/// Everything that just programs the GPU is a generated weak stub in
/// sdk_stubs_gen.c. The draw-done handshake cannot be a stub: sysdolphin
/// sets `drawdone.waiting = 1` and then spins in GXWaitDrawDone() until the
/// draw-done callback clears it (HSD_VIGXSetDrawDone in video.c). With no
/// GPU the frame is "done" as soon as it is submitted, so GXSetDrawDone
/// fires the callback synchronously.

#include <dolphin/gx.h>

#include <port/port.h>

static GXFifoObj fifo;
static GXDrawDoneCallback draw_done_cb;

GXFifoObj* GXInit(void* base, u32 size)
{
    (void) base;
    (void) size;
    return &fifo;
}

GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb)
{
    GXDrawDoneCallback old = draw_done_cb;
    draw_done_cb = cb;
    return old;
}

void GXSetDrawDone(void)
{
    if (draw_done_cb) {
        draw_done_cb();
    }
}

void GXWaitDrawDone(void) {}

void GXDrawDone(void)
{
    GXSetDrawDone();
}
