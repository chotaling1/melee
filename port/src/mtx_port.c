/// Matrix / vector library for the port.
///
/// The SDK sources in libs/dolphin/src/dolphin/mtx implement PSMTX* /
/// PSVEC* with paired-single inline asm, and the C_ fallbacks live in the
/// same files, so they cannot be compiled for x86. These are plain C
/// versions of the same algorithms. mtx44.c (projection matrices) is pure
/// C and is compiled from the SDK directly.
///
/// @todo Verify rounding against the paired-single versions (they use
/// ps_rsqrte + Newton-Raphson for normalization/inverse) once real data
/// is flowing; this matters for physics determinism.

#include <dolphin/mtx.h>

#include <math.h>
#include <string.h>

void PSMTXIdentity(Mtx m)
{
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
}

void PSMTXCopy(Mtx src, Mtx dst)
{
    if (src != dst) {
        memcpy(dst, src, sizeof(Mtx));
    }
}

void PSMTXConcat(Mtx a, Mtx b, Mtx ab)
{
    Mtx t;
    f32(*m)[4] = (ab == a || ab == b) ? t : ab;
    int i;

    for (i = 0; i < 3; i++) {
        m[i][0] = a[i][0] * b[0][0] + a[i][1] * b[1][0] + a[i][2] * b[2][0];
        m[i][1] = a[i][0] * b[0][1] + a[i][1] * b[1][1] + a[i][2] * b[2][1];
        m[i][2] = a[i][0] * b[0][2] + a[i][1] * b[1][2] + a[i][2] * b[2][2];
        m[i][3] = a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] +
                  a[i][3];
    }
    if (m == t) {
        PSMTXCopy(t, ab);
    }
}

void PSMTXTranspose(Mtx src, Mtx xPose)
{
    Mtx t;
    f32(*m)[4] = (src == xPose) ? t : xPose;

    m[0][0] = src[0][0]; m[0][1] = src[1][0]; m[0][2] = src[2][0]; m[0][3] = 0.0f;
    m[1][0] = src[0][1]; m[1][1] = src[1][1]; m[1][2] = src[2][1]; m[1][3] = 0.0f;
    m[2][0] = src[0][2]; m[2][1] = src[1][2]; m[2][2] = src[2][2]; m[2][3] = 0.0f;

    if (m == t) {
        PSMTXCopy(t, xPose);
    }
}

u32 PSMTXInverse(Mtx src, Mtx inv)
{
    Mtx t;
    f32(*m)[4] = (src == inv) ? t : inv;
    f32 det;

    det = src[0][0] * src[1][1] * src[2][2] + src[0][1] * src[1][2] * src[2][0] +
          src[0][2] * src[1][0] * src[2][1] - src[2][0] * src[1][1] * src[0][2] -
          src[1][0] * src[0][1] * src[2][2] - src[0][0] * src[2][1] * src[1][2];

    if (det == 0.0f) {
        return 0;
    }

    det = 1.0f / det;

    m[0][0] = (src[1][1] * src[2][2] - src[2][1] * src[1][2]) * det;
    m[0][1] = -(src[0][1] * src[2][2] - src[2][1] * src[0][2]) * det;
    m[0][2] = (src[0][1] * src[1][2] - src[1][1] * src[0][2]) * det;

    m[1][0] = -(src[1][0] * src[2][2] - src[2][0] * src[1][2]) * det;
    m[1][1] = (src[0][0] * src[2][2] - src[2][0] * src[0][2]) * det;
    m[1][2] = -(src[0][0] * src[1][2] - src[1][0] * src[0][2]) * det;

    m[2][0] = (src[1][0] * src[2][1] - src[2][0] * src[1][1]) * det;
    m[2][1] = -(src[0][0] * src[2][1] - src[2][0] * src[0][1]) * det;
    m[2][2] = (src[0][0] * src[1][1] - src[1][0] * src[0][1]) * det;

    m[0][3] = -m[0][0] * src[0][3] - m[0][1] * src[1][3] - m[0][2] * src[2][3];
    m[1][3] = -m[1][0] * src[0][3] - m[1][1] * src[1][3] - m[1][2] * src[2][3];
    m[2][3] = -m[2][0] * src[0][3] - m[2][1] * src[1][3] - m[2][2] * src[2][3];

    if (m == t) {
        PSMTXCopy(t, inv);
    }
    return 1;
}

void PSMTXScale(Mtx m, f32 xS, f32 yS, f32 zS)
{
    m[0][0] = xS;   m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = yS;   m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = zS;   m[2][3] = 0.0f;
}

void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT)
{
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = xT;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = yT;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = zT;
}

void PSMTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA)
{
    switch (axis) {
    case 'x':
    case 'X':
        m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f;  m[0][3] = 0.0f;
        m[1][0] = 0.0f; m[1][1] = cosA; m[1][2] = -sinA; m[1][3] = 0.0f;
        m[2][0] = 0.0f; m[2][1] = sinA; m[2][2] = cosA;  m[2][3] = 0.0f;
        break;
    case 'y':
    case 'Y':
        m[0][0] = cosA;  m[0][1] = 0.0f; m[0][2] = sinA; m[0][3] = 0.0f;
        m[1][0] = 0.0f;  m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
        m[2][0] = -sinA; m[2][1] = 0.0f; m[2][2] = cosA; m[2][3] = 0.0f;
        break;
    case 'z':
    case 'Z':
        m[0][0] = cosA; m[0][1] = -sinA; m[0][2] = 0.0f; m[0][3] = 0.0f;
        m[1][0] = sinA; m[1][1] = cosA;  m[1][2] = 0.0f; m[1][3] = 0.0f;
        m[2][0] = 0.0f; m[2][1] = 0.0f;  m[2][2] = 1.0f; m[2][3] = 0.0f;
        break;
    default:
        break;
    }
}

void MTXRotRad(Mtx m, char axis, f32 rad)
{
    f32 sinA = sinf(rad);
    f32 cosA = cosf(rad);
    PSMTXRotTrig(m, axis, sinA, cosA);
}

void PSMTXRotAxisRad(Mtx m, Vec* axis, f32 rad)
{
    Vec v;
    f32 s = sinf(rad);
    f32 c = cosf(rad);
    f32 t = 1.0f - c;
    f32 x, y, z, xs, ys, zs, xyt, xzt, yzt;

    PSVECNormalize(axis, &v);
    x = v.x;
    y = v.y;
    z = v.z;
    xs = x * s;
    ys = y * s;
    zs = z * s;
    xyt = x * y * t;
    xzt = x * z * t;
    yzt = y * z * t;

    m[0][0] = x * x * t + c; m[0][1] = xyt - zs;       m[0][2] = xzt + ys;       m[0][3] = 0.0f;
    m[1][0] = xyt + zs;      m[1][1] = y * y * t + c; m[1][2] = yzt - xs;       m[1][3] = 0.0f;
    m[2][0] = xzt - ys;      m[2][1] = yzt + xs;      m[2][2] = z * z * t + c; m[2][3] = 0.0f;
}

void PSMTXQuat(Mtx m, QuaternionPtr q)
{
    f32 s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;

    s = 2.0f / (q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w);

    xs = q->x * s;
    ys = q->y * s;
    zs = q->z * s;
    wx = q->w * xs;
    wy = q->w * ys;
    wz = q->w * zs;
    xx = q->x * xs;
    xy = q->x * ys;
    xz = q->x * zs;
    yy = q->y * ys;
    yz = q->y * zs;
    zz = q->z * zs;

    m[0][0] = 1.0f - (yy + zz); m[0][1] = xy - wz;          m[0][2] = xz + wy;          m[0][3] = 0.0f;
    m[1][0] = xy + wz;          m[1][1] = 1.0f - (xx + zz); m[1][2] = yz - wx;          m[1][3] = 0.0f;
    m[2][0] = xz - wy;          m[2][1] = yz + wx;          m[2][2] = 1.0f - (xx + yy); m[2][3] = 0.0f;
}

void PSMTXMultVec(Mtx m, Vec* src, Vec* dst)
{
    Vec t;
    t.x = m[0][0] * src->x + m[0][1] * src->y + m[0][2] * src->z + m[0][3];
    t.y = m[1][0] * src->x + m[1][1] * src->y + m[1][2] * src->z + m[1][3];
    t.z = m[2][0] * src->x + m[2][1] * src->y + m[2][2] * src->z + m[2][3];
    *dst = t;
}

void PSMTXMultVecSR(Mtx m, Vec* src, Vec* dst)
{
    Vec t;
    t.x = m[0][0] * src->x + m[0][1] * src->y + m[0][2] * src->z;
    t.y = m[1][0] * src->x + m[1][1] * src->y + m[1][2] * src->z;
    t.z = m[2][0] * src->x + m[2][1] * src->y + m[2][2] * src->z;
    *dst = t;
}

void PSMTXMultVecArray(Mtx m, Vec* srcBase, Vec* dstBase, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) {
        PSMTXMultVec(m, &srcBase[i], &dstBase[i]);
    }
}

void C_MTXLookAt(Mtx m, Point3dPtr camPos, VecPtr camUp, Point3dPtr target)
{
    Vec vLook, vRight, vUp;

    vLook.x = camPos->x - target->x;
    vLook.y = camPos->y - target->y;
    vLook.z = camPos->z - target->z;
    PSVECNormalize(&vLook, &vLook);

    PSVECCrossProduct(camUp, &vLook, &vRight);
    PSVECNormalize(&vRight, &vRight);

    PSVECCrossProduct(&vLook, &vRight, &vUp);

    m[0][0] = vRight.x;
    m[0][1] = vRight.y;
    m[0][2] = vRight.z;
    m[0][3] = -(camPos->x * vRight.x + camPos->y * vRight.y + camPos->z * vRight.z);

    m[1][0] = vUp.x;
    m[1][1] = vUp.y;
    m[1][2] = vUp.z;
    m[1][3] = -(camPos->x * vUp.x + camPos->y * vUp.y + camPos->z * vUp.z);

    m[2][0] = vLook.x;
    m[2][1] = vLook.y;
    m[2][2] = vLook.z;
    m[2][3] = -(camPos->x * vLook.x + camPos->y * vLook.y + camPos->z * vLook.z);
}

void MTXLightFrustum(Mtx m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 scaleS,
                     f32 scaleT, f32 transS, f32 transT)
{
    f32 tmp;

    tmp = 1.0f / (r - l);
    m[0][0] = ((2 * n) * tmp) * scaleS;
    m[0][1] = 0.0f;
    m[0][2] = (((r + l) * tmp) * scaleS) - transS;
    m[0][3] = 0.0f;

    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = ((2 * n) * tmp) * scaleT;
    m[1][2] = (((t + b) * tmp) * scaleT) - transT;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = -1.0f;
    m[2][3] = 0.0f;
}

void MTXLightPerspective(Mtx m, f32 fovY, f32 aspect, f32 scaleS, f32 scaleT,
                         f32 transS, f32 transT)
{
    f32 angle;
    f32 cot;

    angle = fovY * 0.5f;
    angle = MTXDegToRad(angle);
    cot = 1.0f / tanf(angle);

    m[0][0] = (cot / aspect) * scaleS;
    m[0][1] = 0.0f;
    m[0][2] = -transS;
    m[0][3] = 0.0f;

    m[1][0] = 0.0f;
    m[1][1] = cot * scaleT;
    m[1][2] = -transT;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = -1.0f;
    m[2][3] = 0.0f;
}

void MTXLightOrtho(Mtx m, f32 t, f32 b, f32 l, f32 r, f32 scaleS, f32 scaleT,
                   f32 transS, f32 transT)
{
    f32 tmp;

    tmp = 1.0f / (r - l);
    m[0][0] = (2.0f * tmp * scaleS);
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = ((-(r + l) * tmp) * scaleS) + transS;

    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = (2.0f * tmp) * scaleT;
    m[1][2] = 0.0f;
    m[1][3] = ((-(t + b) * tmp) * scaleT) + transT;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 0.0f;
    m[2][3] = 1.0f;
}

/* ---- vectors ---- */

void PSVECAdd(Vec* a, Vec* b, Vec* c)
{
    c->x = a->x + b->x;
    c->y = a->y + b->y;
    c->z = a->z + b->z;
}

void PSVECSubtract(Vec* a, Vec* b, Vec* c)
{
    c->x = a->x - b->x;
    c->y = a->y - b->y;
    c->z = a->z - b->z;
}

void PSVECScale(Vec* src, Vec* dst, f32 scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

f32 PSVECSquareMag(Vec* v)
{
    return v->x * v->x + v->y * v->y + v->z * v->z;
}

f32 PSVECMag(Vec* v)
{
    return sqrtf(PSVECSquareMag(v));
}

void PSVECNormalize(Vec* src, Vec* unit)
{
    f32 mag = PSVECSquareMag(src);
    if (mag <= 0.0f) {
        unit->x = unit->y = unit->z = 0.0f;
        return;
    }
    mag = 1.0f / sqrtf(mag);
    unit->x = src->x * mag;
    unit->y = src->y * mag;
    unit->z = src->z * mag;
}

f32 PSVECDotProduct(Vec* a, Vec* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

void PSVECCrossProduct(Vec* a, Vec* b, Vec* axb)
{
    Vec t;
    t.x = a->y * b->z - a->z * b->y;
    t.y = a->z * b->x - a->x * b->z;
    t.z = a->x * b->y - a->y * b->x;
    *axb = t;
}

f32 PSVECSquareDistance(Vec* a, Vec* b)
{
    f32 dx = a->x - b->x;
    f32 dy = a->y - b->y;
    f32 dz = a->z - b->z;
    return dx * dx + dy * dy + dz * dz;
}
