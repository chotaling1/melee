/// GX pieces that need real behaviour even with no renderer.
///
/// Everything that just programs the GPU is a generated weak stub in
/// sdk_stubs_gen.c. The draw-done handshake cannot be a stub: sysdolphin
/// sets `drawdone.waiting = 1` and then spins in GXWaitDrawDone() until the
/// draw-done callback clears it (HSD_VIGXSetDrawDone in video.c). With no
/// GPU the frame is "done" as soon as it is submitted, so GXSetDrawDone
/// fires the callback synchronously.

#include <dolphin/gx.h>
#include <dolphin/mtx.h>

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

/// Game code projects world positions to screen space with GXProject
/// (lbVector_WorldToScreen: on-screen tests, magnifier, camera). It is pure
/// math, so use the SDK's algorithm; an empty stub left the caller reading
/// uninitialized stack, which made gameplay differ between builds.
void GXProject(f32 x, f32 y, f32 z, f32 mtx[3][4], f32* pm, f32* vp, f32* sx,
               f32* sy, f32* sz)
{
    Vec peye;
    f32 xc;
    f32 yc;
    f32 zc;
    f32 wc;

    peye.x =
        mtx[0][3] + ((mtx[0][2] * z) + ((mtx[0][0] * x) + (mtx[0][1] * y)));
    peye.y =
        mtx[1][3] + ((mtx[1][2] * z) + ((mtx[1][0] * x) + (mtx[1][1] * y)));
    peye.z =
        mtx[2][3] + ((mtx[2][2] * z) + ((mtx[2][0] * x) + (mtx[2][1] * y)));
    if (pm[0] == 0.0f) {
        xc = (peye.x * pm[1]) + (peye.z * pm[2]);
        yc = (peye.y * pm[3]) + (peye.z * pm[4]);
        zc = pm[6] + (peye.z * pm[5]);
        wc = 1.0f / -peye.z;
    } else {
        xc = pm[2] + (peye.x * pm[1]);
        yc = pm[4] + (peye.y * pm[3]);
        zc = pm[6] + (peye.z * pm[5]);
        wc = 1.0f;
    }
    *sx = (vp[2] / 2.0f) + (vp[0] + (wc * (xc * vp[2] / 2.0f)));
    *sy = (vp[3] / 2.0f) + (vp[1] + (wc * (-yc * vp[3] / 2.0f)));
    *sz = vp[5] + (wc * (zc * (vp[5] - vp[4])));
}

/* Projection and viewport state, kept so the Get* calls (fog, particles)
 * return what was set, as the SDK's shadow copies do. */
static f32 proj_state[7];
static f32 viewport_state[6];

void GXSetProjection(f32 mtx[4][4], GXProjectionType type)
{
    proj_state[0] = (f32) type;
    proj_state[1] = mtx[0][0];
    proj_state[3] = mtx[1][1];
    proj_state[5] = mtx[2][2];
    proj_state[6] = mtx[2][3];
    if (type == GX_ORTHOGRAPHIC) {
        proj_state[2] = mtx[0][3];
        proj_state[4] = mtx[1][3];
    } else {
        proj_state[2] = mtx[0][2];
        proj_state[4] = mtx[1][2];
    }
}

void GXGetProjectionv(f32* ptr)
{
    int i;
    for (i = 0; i < 7; i++) {
        ptr[i] = proj_state[i];
    }
}

void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz,
                         f32 farz, u32 field)
{
    if (field == 0) {
        top -= 0.5f;
    }
    viewport_state[0] = left;
    viewport_state[1] = top;
    viewport_state[2] = wd;
    viewport_state[3] = ht;
    viewport_state[4] = nearz;
    viewport_state[5] = farz;
}

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz)
{
    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 1);
}

void GXGetViewportv(f32* vp)
{
    int i;
    for (i = 0; i < 6; i++) {
        vp[i] = viewport_state[i];
    }
}
