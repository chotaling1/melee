/// Data symbols the game links against that normally come from the SDK
/// or MSL data sections.

#include <dolphin/gx.h>

#include <math.h>

/// Render modes, copied from libs/dolphin/src/dolphin/gx/GXFrameBuf.c
/// (that file also programs the GPU, so it is not compiled into the port).
GXRenderModeObj GXNtsc480IntDf = { 0,
                                   640,
                                   480,
                                   480,
                                   40,
                                   0,
                                   640,
                                   480,
                                   1,
                                   0,
                                   0,
                                   { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
                                   { 8, 8, 10, 12, 10, 8, 8 } };

GXRenderModeObj GXNtsc480Int = { 0,
                                 640,
                                 480,
                                 480,
                                 40,
                                 0,
                                 640,
                                 480,
                                 1,
                                 0,
                                 0,
                                 { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                   6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
                                 { 0, 0, 21, 22, 21, 0, 0 } };

GXRenderModeObj GXNtsc480Prog = { 2,
                                  640,
                                  480,
                                  480,
                                  40,
                                  0,
                                  640,
                                  480,
                                  0,
                                  0,
                                  0,
                                  { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
                                  { 0, 0, 21, 22, 21, 0, 0 } };

/// MSL's float NAN / INF constants (src/melee/lb/lbtrigf.c reads them as
/// MSL_TrigF_80400770[0] and MSL_TrigF_80400774[0]).
float MSL_TrigF_80400770[1] = { NAN };
float MSL_TrigF_80400774[1] = { INFINITY };

/// Linker-provided stack bounds on the GameCube. Only db_PrintThreadInfo
/// reads them (to report stack usage), so a small dummy region is enough.
unsigned char _stack_end[0x100];
unsigned char _stack_addr[4];
