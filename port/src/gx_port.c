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

/* ---- texture buffer math (from libs/dolphin/src/dolphin/gx/GXTexture.c,
 * which also touches GPU registers and so is not compiled directly) ---- */

static void get_tex_tile_shift(GXTexFmt fmt, u32* rowTileS, u32* colTileS)
{
    switch (fmt) {
    case GX_TF_I4:
    case 0x8:
    case GX_TF_CMPR:
    case GX_CTF_R4:
    case GX_CTF_Z4:
        *rowTileS = 3;
        *colTileS = 3;
        break;
    case GX_TF_I8:
    case GX_TF_IA4:
    case 0x9:
    case GX_TF_Z8:
    case GX_CTF_RA4:
    case GX_TF_A8:
    case GX_CTF_R8:
    case GX_CTF_G8:
    case GX_CTF_B8:
    case GX_CTF_Z8M:
    case GX_CTF_Z8L:
        *rowTileS = 3;
        *colTileS = 2;
        break;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_RGBA8:
    case 0xA:
    case GX_TF_Z16:
    case GX_TF_Z24X8:
    case GX_CTF_RA8:
    case GX_CTF_RG8:
    case GX_CTF_GB8:
    case GX_CTF_Z16L:
        *rowTileS = 2;
        *colTileS = 2;
        break;
    default:
        *rowTileS = *colTileS = 0;
        break;
    }
}

u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, u8 mipmap,
                       u8 max_lod)
{
    u32 tileShiftX, tileShiftY, tileBytes, bufferSize, nx, ny, level;

    get_tex_tile_shift(format, &tileShiftX, &tileShiftY);
    tileBytes = (format == GX_TF_RGBA8 || format == GX_TF_Z24X8) ? 64 : 32;
    if (mipmap == 1) {
        bufferSize = 0;
        for (level = 0; level < max_lod; level++) {
            nx = (width + (1 << tileShiftX) - 1) >> tileShiftX;
            ny = (height + (1 << tileShiftY) - 1) >> tileShiftY;
            bufferSize += tileBytes * (nx * ny);
            if (width == 1 && height == 1) {
                break;
            }
            width = (width > 1) ? width >> 1 : 1;
            height = (height > 1) ? height >> 1 : 1;
        }
    } else {
        nx = (width + (1 << tileShiftX) - 1) >> tileShiftX;
        ny = (height + (1 << tileShiftY) - 1) >> tileShiftY;
        bufferSize = nx * ny * tileBytes;
    }
    return bufferSize;
}

void GXDrawDone(void)
{
    GXSetDrawDone();
}
