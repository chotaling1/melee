/// Synthetic "line" stage: Final Destination's gameplay geometry built
/// natively, with no stage DAT.
///
///     MELEE_PORT_STAGE=line
///
/// replaces FD (St_Kind_Last -> Gr_Kind_Last). Keeping FD's ground and stage
/// kinds means every grkind/stkind check in game code (camera, items, BGM
/// lookup) sees FD, while nothing is read from GrNLa.dat.
///
/// How a stage normally comes up (src/melee/gr/ground.c):
///   Ground_801C0754  grDatFiles_801C6038(data1) pulls coll_data,
///                    grGroundParam, ... out of the DAT's map_head; a NULL
///                    data1 takes the built-in DAT-less path (empty
///                    map_head, default GroundParam, no collision).
///   Ground_801C0800  camera params from GroundParam, mpLibLoad(coll_data),
///                    fog/lights (defaults when map_head has no models),
///                    then StageData::on_init.
///   on_init          map gobjs; Ground_801C34AC fills stage_info.x280[id]
///                    with the model's marker joints ("general points"),
///                    which Ground_801C39C0 (camera range, ids 0x94-0x96)
///                    and Ground_801C3BB4 (blast zones, 0x97/0x98) read.
///
/// Here data1 is NULL, port_stage_load() swaps in native GroundParam and
/// MapCollData, and on_init creates one parentless JObj per general point.
///
/// All numbers below were read from GrNLa.dat (NTSC v1.02) on a local disc
/// and transcribed by hand; no DAT bytes are embedded.

#include <stdlib.h>
#include <string.h>

#include <port/port.h>
#include <port/stage.h>

#include <melee/cm/camera.h>
#include <melee/gr/ground.h>
#include <melee/gr/types.h>
#include <melee/mp/forward.h>
#include <melee/mp/mplib.h>
#include <melee/mp/types.h>
#include <sysdolphin/baselib/jobj.h>

/* ---- Collision (GrNLa.dat coll_data) ---------------------------------- */

static Vec2 line_verts[] = {
    /*  0 */ { -65.7993F, -20.4538F },
    /*  1 */ { -85.5657F, -10.5F },
    /*  2 */ { 85.5657F, -10.5F },
    /*  3 */ { 65.7993F, -20.4538F },
    /*  4 */ { -85.5657F, 0.0F },
    /*  5 */ { 85.5657F, 0.0F },
    /*  6 */ { 75.0F, 0.0F },
    /*  7 */ { -47.456F, -55.3882F },
    /*  8 */ { -53.7736F, -54.2584F },
    /*  9 */ { 47.456F, -55.3882F },
    /* 10 */ { 65.8374F, -31.3443F },
    /* 11 */ { 61.4195F, -47.3663F },
    /* 12 */ { -61.4195F, -47.3663F },
    /* 13 */ { -65.8374F, -31.3443F },
    /* 14 */ { 53.7736F, -54.2584F },
    /* 15 */ { -75.0F, 0.0F },
};

#define FLOOR CollLine_Floor
#define CEIL CollLine_Ceiling
#define RWALL CollLine_RightWall
#define LWALL CollLine_LeftWall
#define LEDGE LINE_FLAG_LEDGE

/// v0, v1, prev, next, prev1, next1, kind, ledge/platform flags.
/// Grouped by kind: floors 0-2, ceilings 3-5, right walls 6-10,
/// left walls 11-15. The two outer floor segments carry the ledges.
static MapLine line_lines[] = {
    /*  0 */ { 4, 15, 11, 1, -1, -1, FLOOR, LEDGE },
    /*  1 */ { 15, 6, 0, 2, -1, -1, FLOOR, 0 },
    /*  2 */ { 6, 5, 1, 9, -1, -1, FLOOR, LEDGE },
    /*  3 */ { 7, 8, 4, 15, -1, -1, CEIL, 0 },
    /*  4 */ { 9, 7, 5, 3, -1, -1, CEIL, 0 },
    /*  5 */ { 14, 9, 6, 4, -1, -1, CEIL, 0 },
    /*  6 */ { 11, 14, 8, 5, -1, -1, RWALL, 0 },
    /*  7 */ { 3, 10, 10, 8, -1, -1, RWALL, 0 },
    /*  8 */ { 10, 11, 7, 6, -1, -1, RWALL, 0 },
    /*  9 */ { 5, 2, 2, 10, -1, -1, RWALL, 0 },
    /* 10 */ { 2, 3, 9, 7, -1, -1, RWALL, 0 },
    /* 11 */ { 1, 4, 13, 0, -1, -1, LWALL, 0 },
    /* 12 */ { 13, 0, 14, 13, -1, -1, LWALL, 0 },
    /* 13 */ { 0, 1, 12, 11, -1, -1, LWALL, 0 },
    /* 14 */ { 12, 13, 15, 12, -1, -1, LWALL, 0 },
    /* 15 */ { 8, 12, 3, 14, -1, -1, LWALL, 0 },
};

static MapJoint line_joints[] = { {
    0, 3,   /* floor */
    3, 3,   /* ceiling */
    6, 5,   /* right wall */
    11, 5,  /* left wall */
    0, 0,   /* dynamic */
    -93.5657F, -63.3882F, 93.5657F, 8.0F,
    0, ARRAY_SIZE(line_verts),
} };

static MapCollData line_coll = {
    line_verts, ARRAY_SIZE(line_verts),
    line_lines, ARRAY_SIZE(line_lines),
    0, 3, 3, 3, 6, 5, 11, 5, 0, 0,
    line_joints, ARRAY_SIZE(line_joints),
    0,
};

/* ---- Ground params (GrNLa.dat grGroundParam) -------------------------- */

static StageParam line_stage_params[] = { {
    .stkind = St_Kind_Last,
    .x4 = 78, /* BGM */
    .x8 = 39, /* alternate BGM */
    .xC = 78,
    .x10 = 39,
    .x14 = 6,
    .x16 = 12,
    .x18 = 100,
    /* Per-item spawn switches (1 = may spawn here), indexed by item kind
     * and read by Ground_801C28CC. */
    .x1A = { 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 },
} };

#define LINE_BG { 0x0C, 0x06, 0x28, 0xFF }

static GroundParam line_param = {
    .y = 1.0F, /* stage scale */
    .x4 = 192,
    .x8 = 30,     /* camera vertical tilt */
    .xC = 83,     /* camera zoom rate */
    .x10 = 1000,  /* camera max depth */
    .x14 = -10,   /* camera pan degrees */
    .x18 = 0.05F, .x1C = 0.05F,
    .x20 = 1.5F,  /* track ratio */
    .x24 = 1.5F,  /* fixed zoom */
    .x28 = 1.8F,  /* track smooth */
    .x2E = 60, .x30 = 40, .x34 = 100, .x38 = 200, /* pause camera */
    .x3C = 0, .x40 = 45, .x44 = 68, .x48 = 68,    /* pause camera angles */
    .x4C_fixed_cam = false,
    .x50 = 0, .x54 = 45, .x58 = 356, .x5C = 30, .x60 = -2, .x64 = 0,
    .x68 = 100,
    /* Item spawn weights per item kind, read by Ground_801C28CC. Only
     * used when the match rules turn items on (MELEE_PORT_DEMO_ITEMS). */
    .x6A = { 60, 60, 40, 0, 30, 6, 20, 8, 3, 8, 7, 7, 16, 8, 10, 7, 14, 8,
             20, 16, 10, 16, 10, 9, 15, 12, 7, 5, 6, 15, 12, 10, 7, 10, 40 },
    .stage_params = line_stage_params,
    .stage_param_count = ARRAY_SIZE(line_stage_params),
    .xB8 = LINE_BG, .xBC = LINE_BG, .xC0 = LINE_BG,
    .xC4 = LINE_BG, .xC8 = { 0x78, 0x06, 0x28, 0xFF }, .xCC = LINE_BG,
    .xD0 = LINE_BG, .xD4 = LINE_BG, .xD8 = LINE_BG,
};

/* ---- General points (GrNLa.dat map_head joint markers) ---------------- */

typedef struct {
    int id;
    float x, y;
} GeneralPoint;

static const GeneralPoint line_points[] = {
    { 0x00, -60, 10 },   { 0x01, 60, 10 },    /* player spawns */
    { 0x02, -20, 10 },   { 0x03, 20, 10 },
    { 0x04, 16, 45 },    { 0x05, -50, 45 },   /* respawn platforms */
    { 0x06, 50, 45 },    { 0x07, -15, 45 },
    { 0x7F, 0, 30 },     { 0x80, -75, 30 },   /* item/misc points */
    { 0x81, 75, 30 },    { 0x82, 0, 60 },
    { 0x83, -50, 70 },   { 0x84, 50, 70 },
    { 0x85, -25, 45 },   { 0x86, 25, 45 },
    { 0x94, 0, 12 },                          /* camera center */
    { 0x95, -170, 114 }, { 0x96, 170, -80 },  /* camera range */
    { 0x97, -246, 188 }, { 0x98, 246, -140 }, /* blast zones */
};

/* ---- StageData -------------------------------------------------------- */

static void line_OnInit(void)
{
    size_t i;
    for (i = 0; i < ARRAY_SIZE(line_points); i++) {
        HSD_Joint joint = {
            NULL,        0,           NULL,
            NULL,        { NULL },    { 0, 0, 0 },
            { 1, 1, 1 }, { 0, 0, 0 }, NULL,
            NULL,
        };
        joint.position.x = line_points[i].x;
        joint.position.y = line_points[i].y;
        stage_info.x280[line_points[i].id] = HSD_JObjLoadJoint(&joint);
    }
    /* Same as grLast_OnInit, minus the model gobjs. */
    stage_info.unk8C.b4 = 1;
    stage_info.unk8C.b5 = 1;
    Ground_801C39C0();
    Ground_801C3BB4();
    Camera_800311DC(1);
    Camera_800311CC(30000);
    port_log("stage line: cam [%g,%g]x[%g,%g] off (%g,%g), "
             "blast [%g,%g]x[%g,%g]",
             stage_info.cam_info.cam_bounds.left,
             stage_info.cam_info.cam_bounds.right,
             stage_info.cam_info.cam_bounds.bottom,
             stage_info.cam_info.cam_bounds.top,
             stage_info.cam_info.cam_x_offset,
             stage_info.cam_info.cam_y_offset, stage_info.blast_zone.left,
             stage_info.blast_zone.right, stage_info.blast_zone.bottom,
             stage_info.blast_zone.top);

    /* Sanity probes through the real collision code (mpLibLoad ran). */
    {
        static const float probes[][4] = {
            { 0, 10, 0, -10 },     /* center of the floor */
            { 80, 10, 80, -10 },   /* outer right floor segment (ledge) */
            { -120, -5, -60, -5 }, /* into the left wall */
        };
        for (i = 0; i < ARRAY_SIZE(probes); i++) {
            Vec3 pos = { 0 };
            int line = -1;
            u32 flags = 0;
            bool hit = mpCheckMultiple(probes[i][0], probes[i][1],
                                       probes[i][2], probes[i][3], &pos,
                                       &line, &flags, NULL, 0xF, -1, -1);
            port_log("stage line: probe (%g,%g)->(%g,%g): hit=%d line=%d "
                     "kind=%#x flags=%#x at (%g,%g)",
                     probes[i][0], probes[i][1], probes[i][2], probes[i][3],
                     hit, line, hit ? mpLineGetKind(line) : 0, flags, pos.x,
                     pos.y);
        }
    }
}

static void line_OnDemoInit(int arg) {}

static void line_Nop(void) {}

static bool line_Callback4(void)
{
    return false;
}

static DynamicsDesc* line_OnTouchLine(int line_id)
{
    return NULL;
}

static bool line_OnCheckShadowRender(Vec3* pos, int arg, HSD_JObj* jobj)
{
    return true;
}

/// Only indexed for map ids below map_head's model count, which is 0 here.
static StageCallbacks line_callbacks[1];

static StageData line_StageData = {
    Gr_Kind_Last,
    line_callbacks,
    NULL, /* no DAT */
    line_OnInit,
    line_OnDemoInit,
    line_Nop,
    line_Nop,
    line_Callback4,
    line_OnTouchLine,
    line_OnCheckShadowRender,
    (1 << 0),
    NULL,
    0,
};

StageData* port_stage_override(GrKind grkind, StageData* orig)
{
    static int enabled = -1;
    if (enabled < 0) {
        const char* s = getenv("MELEE_PORT_STAGE");
        enabled = s != NULL && strcmp(s, "line") == 0;
        if (enabled) {
            port_log("MELEE_PORT_STAGE=line: synthetic stage replaces "
                     "Final Destination");
        }
    }
    if (enabled && grkind == Gr_Kind_Last) {
        return &line_StageData;
    }
    return orig;
}

void port_stage_load(StageData* stage)
{
    if (stage != &line_StageData) {
        return;
    }
    stage_info.param = &line_param;
    stage_info.coll_data = &line_coll;
    port_log("stage line: %d verts, %d lines, %d joint(s), no DAT",
             line_coll.vert_count, line_coll.line_count,
             line_coll.joint_count);
}
