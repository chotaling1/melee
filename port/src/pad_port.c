/// PAD (controller) for the headless port: no controllers connected.
///
/// PADRead must fill every slot; db_GetGameLaunchButtonState loops until
/// no slot reports PAD_ERR_NOT_READY / PAD_ERR_TRANSFER. SDL input plugs
/// in here later (roadmap step 3).

#include <dolphin/pad.h>

#include <string.h>

BOOL PADInit(void)
{
    return TRUE;
}

u32 PADRead(PADStatus* status)
{
    int i;
    memset(status, 0, sizeof(PADStatus) * PAD_MAX_CONTROLLERS);
    for (i = 0; i < PAD_MAX_CONTROLLERS; i++) {
        status[i].err = PAD_ERR_NO_CONTROLLER;
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
