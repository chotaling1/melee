/// PAD (controllers) for the headless port.
///
/// With no input configured, no controllers are connected. For headless
/// testing, MELEE_PORT_INPUT scripts port 1:
///
///     MELEE_PORT_INPUT="120:A,300:START:5,400:DOWN+A"
///
/// Each entry is RETRACE:BUTTONS[:DURATION] (duration in retraces,
/// default 3). Buttons are joined with '+': A B X Y Z L R START UP DOWN
/// LEFT RIGHT. When a script is set, port 1 reports connected for the whole
/// run. PADRead must fill every slot; db_GetGameLaunchButtonState loops
/// until no slot reports PAD_ERR_NOT_READY / PAD_ERR_TRANSFER. SDL input
/// replaces this in roadmap step 3.

#include <dolphin/pad.h>
#include <dolphin/vi.h>

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <port/port.h>

#define MAX_EVENTS 256

typedef struct {
    u32 start;
    u32 duration;
    u16 buttons;
} PadEvent;

static PadEvent events[MAX_EVENTS];
static int nevents;
static int scripted;
static int script_loaded;

static u16 parse_buttons(const char* s, const char* end)
{
    static const struct {
        const char* name;
        u16 bit;
    } names[] = {
        { "A", PAD_BUTTON_A },         { "B", PAD_BUTTON_B },
        { "X", PAD_BUTTON_X },         { "Y", PAD_BUTTON_Y },
        { "Z", PAD_TRIGGER_Z },        { "L", PAD_TRIGGER_L },
        { "R", PAD_TRIGGER_R },        { "START", PAD_BUTTON_START },
        { "UP", PAD_BUTTON_UP },       { "DOWN", PAD_BUTTON_DOWN },
        { "LEFT", PAD_BUTTON_LEFT },   { "RIGHT", PAD_BUTTON_RIGHT },
    };
    u16 bits = 0;
    while (s < end) {
        const char* tok_end = s;
        size_t len;
        unsigned i;
        while (tok_end < end && *tok_end != '+') {
            tok_end++;
        }
        len = (size_t) (tok_end - s);
        for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
            if (strlen(names[i].name) == len &&
                strncasecmp(names[i].name, s, len) == 0)
            {
                bits |= names[i].bit;
                break;
            }
        }
        if (i == sizeof(names) / sizeof(names[0])) {
            port_log("MELEE_PORT_INPUT: unknown button '%.*s'", (int) len, s);
        }
        s = tok_end < end ? tok_end + 1 : tok_end;
    }
    return bits;
}

static void load_script(void)
{
    const char* p = getenv("MELEE_PORT_INPUT");
    if (script_loaded) {
        return;
    }
    script_loaded = 1;
    if (!p) {
        return;
    }
    scripted = 1;
    while (*p && nevents < MAX_EVENTS) {
        PadEvent* ev = &events[nevents];
        char* q;
        const char* entry_end = strchr(p, ',');
        const char* btn_start;
        const char* btn_end;
        if (!entry_end) {
            entry_end = p + strlen(p);
        }
        ev->start = (u32) strtoul(p, &q, 10);
        if (*q != ':') {
            port_log("MELEE_PORT_INPUT: bad entry '%.*s'",
                     (int) (entry_end - p), p);
            break;
        }
        btn_start = q + 1;
        btn_end = btn_start;
        while (btn_end < entry_end && *btn_end != ':') {
            btn_end++;
        }
        ev->buttons = parse_buttons(btn_start, btn_end);
        ev->duration = btn_end < entry_end
                           ? (u32) strtoul(btn_end + 1, NULL, 10)
                           : 3;
        nevents++;
        p = *entry_end ? entry_end + 1 : entry_end;
    }
    port_log("MELEE_PORT_INPUT: %d events", nevents);
}

BOOL PADInit(void)
{
    load_script();
    return TRUE;
}

u32 PADRead(PADStatus* status)
{
    int i;
    memset(status, 0, sizeof(PADStatus) * PAD_MAX_CONTROLLERS);
    for (i = 0; i < PAD_MAX_CONTROLLERS; i++) {
        status[i].err = PAD_ERR_NO_CONTROLLER;
    }
    if (scripted) {
        u32 now = VIGetRetraceCount();
        status[0].err = PAD_ERR_NONE;
        for (i = 0; i < nevents; i++) {
            if (now >= events[i].start &&
                now < events[i].start + events[i].duration)
            {
                status[0].button |= events[i].buttons;
            }
        }
        return PAD_CHAN0_BIT;
    }
    return 0;
}

void PADClamp(PADStatus* status)
{
    (void) status;
}

int PADReset(unsigned long mask)
{
    (void) mask;
    return TRUE;
}

BOOL PADRecalibrate(u32 mask)
{
    (void) mask;
    return TRUE;
}

void PADSetSpec(u32 spec)
{
    (void) spec;
}

void PADSetSamplingRate(unsigned long msec)
{
    (void) msec;
}

void PADControlMotor(s32 chan, u32 command)
{
    (void) chan;
    (void) command;
}
