/// MSL float math pieces that MWCC provided as intrinsics or header inlines.
///
/// The rest of the game's float math is built from the decompiled MSL
/// (src/MSL/trigf.c, math.c) and lb/lbtrigf.c, so sinf/cosf/tanf/atanf/logf
/// give the same results on every host instead of whatever the host libm
/// computes (musl on Linux, MinGW on Windows).

#include <MetroTRK/intrinsics.h>

typedef union {
    double d;
    unsigned long long u;
} port_dbits;

/// PPC fnmsubs: -(a * b - c) with a single rounding, as the fused
/// instruction does. a * b is exact in a double (two 24-bit mantissas);
/// c - a * b is split into a rounded double s and its exact error e
/// (TwoSum), so the only case where rounding s to float could go the wrong
/// way, s lying exactly between two floats, is settled by the sign of e.
float __fnmsubs(float a, float b, float c)
{
    double p = (double) a * (double) b;
    double s = (double) c - p;
    double bv = s - (double) c;
    double e = ((double) c - (s - bv)) + (-p - bv);
    float f = (float) s;

    if (e != 0.0 && (double) f != s) {
        /* Neighbouring float on the other side of s. */
        float g = (double) f < s ? __builtin_nextafterf(f, __builtin_inff())
                                 : __builtin_nextafterf(f, -__builtin_inff());
        double mid = ((double) f + (double) g) / 2.0;
        if (s == mid) {
            if ((e > 0.0) == ((double) g > (double) f)) {
                f = g;
            }
        }
    }
    return f;
}

/// MSL math.h inline: truncating quotient through s64, not IEEE fmod.
float fmodf(float a, float b)
{
    long long quotient;

    if (__fabsf(b) > __fabsf(a)) {
        return a;
    }
    quotient = a / b;
    return a - b * quotient;
}

float __fabsf(float x)
{
    return x < 0.0f ? -x : (x == 0.0f ? 0.0f : x);
}

float fabsf__Ff(float x)
{
    return __fabsf(x);
}
