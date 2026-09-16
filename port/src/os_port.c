/// OS layer for the port: memory map, console output, time, alarms.
///
/// Heaps (OSAlloc.c) and the arena bump allocator (OSArena.c) are the
/// SDK's own C code, compiled straight from libs/dolphin.

#include <dolphin/os.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include <port/port.h>

/* ---- logging ---- */

void port_log(const char* fmt, ...)
{
    va_list ap;
    fputs("[port] ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

void OSReport(char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void OSPanic(const char* file, int line, const char* msg, ...)
{
    va_list ap;
    fprintf(stderr, "OSPanic: %s:%d: ", file, line);
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fputc('\n', stderr);
    abort();
}

static OSErrorHandler error_handlers[OS_ERROR_MAX];

OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler)
{
    OSErrorHandler old = NULL;
    if (error < OS_ERROR_MAX) {
        old = error_handlers[error];
        error_handlers[error] = handler;
    }
    return old;
}

/* ---- memory ---- */

/// Low-memory OS globals (see OS_BASE_CACHED offsets in dolphin/os.h).
#define LOMEM_U32(off) (*(volatile u32*) (PORT_MEM1_BASE + (off)))

/// Where the arena starts inside MEM1. On the console this is the end of
/// the DOL's .bss (set by the apploader); the port's static data lives in
/// the ELF instead, so the first 6 MB are simply left unused.
/// @todo Use the real GALE01 __ArenaLo so heap addresses line up with
/// Dolphin for step-2 comparisons.
#define PORT_ARENA_LO (PORT_MEM1_BASE + 0x00600000u)

void* port_mem1_ptr(u32 addr)
{
    if (addr < PORT_MEM1_BASE) {
        addr |= PORT_MEM1_BASE; /* physical -> cached */
    }
    return (void*) addr;
}

static void map_mem1(void)
{
    void* want = (void*) PORT_MEM1_BASE;
    void* got = mmap(want, PORT_MEM1_SIZE, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (got != want) {
        port_log("cannot map MEM1 at %p (got %p)", want, got);
        abort();
    }

    LOMEM_U32(0x0020) = 0x0D15EA5E; /* boot magic */
    LOMEM_U32(0x0028) = PORT_MEM1_SIZE; /* physical mem size */
    LOMEM_U32(0x002C) = 0x00000003; /* console type: retail */
    LOMEM_U32(0x0030) = PORT_ARENA_LO;
    LOMEM_U32(0x0034) = PORT_MEM1_BASE + PORT_MEM1_SIZE;
    LOMEM_U32(0x00CC) = 0; /* TV mode: NTSC */
    LOMEM_U32(0x00F0) = PORT_MEM1_SIZE; /* simulated mem size */
    LOMEM_U32(0x00F8) = PORT_TIMER_CLOCK * 4; /* bus clock, 162 MHz */
    LOMEM_U32(0x00FC) = PORT_TIMER_CLOCK * 12; /* core clock, 486 MHz */
}

u32 OSGetConsoleSimulatedMemSize(void)
{
    return PORT_MEM1_SIZE;
}

u32 OSGetPhysicalMemSize(void)
{
    return PORT_MEM1_SIZE;
}

void DCFlushRange(void* addr, u32 nBytes)
{
    (void) addr;
    (void) nBytes;
}

void DCInvalidateRange(void* addr, u32 nBytes)
{
    (void) addr;
    (void) nBytes;
}

void DCStoreRange(void* addr, u32 nBytes)
{
    (void) addr;
    (void) nBytes;
}

/* ---- time ---- */

/// Virtual timebase. It advances one NTSC field per VIWaitForRetrace, plus
/// a few ticks per read so that spin-waits on OSGetTick() terminate. This
/// keeps a headless run deterministic no matter how fast the host is; a
/// windowed build paces the retraces to real time instead of the clock.
static OSTime port_ticks;

static void init_time(void)
{
    /* Start from the wall clock so the calendar the game prints is real.
     * GameCube epoch is 2000-01-01 00:00:00. */
    time_t now = time(NULL);
    port_ticks = (OSTime) (now - 946684800) * PORT_TIMER_CLOCK;
}

static void flush_deferred(void);

OSTick OSGetTick(void)
{
    port_ticks += 8;
    flush_deferred();
    return (OSTick) port_ticks;
}

OSTime OSGetTime(void)
{
    port_ticks += 8;
    flush_deferred();
    return port_ticks;
}

static int is_leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td)
{
    static const int mdays[12] = { 31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31 };
    OSTime secs = ticks / PORT_TIMER_CLOCK;
    OSTime rem = ticks - secs * PORT_TIMER_CLOCK;
    int days = (int) (secs / 86400);
    int daysecs = (int) (secs % 86400);
    int year = 2000;
    int mon = 0;

    td->usec = (int) (rem * 8 / (PORT_TIMER_CLOCK / 125000)) % 1000;
    td->msec = (int) (rem / (PORT_TIMER_CLOCK / 1000)) % 1000;
    td->sec = daysecs % 60;
    td->min = (daysecs / 60) % 60;
    td->hour = daysecs / 3600;
    td->wday = (days + 6) % 7; /* 2000-01-01 was a Saturday */

    while (days >= 365 + is_leap(year)) {
        days -= 365 + is_leap(year);
        year++;
    }
    td->yday = days;
    while (days >= mdays[mon] + (mon == 1 && is_leap(year))) {
        days -= mdays[mon] + (mon == 1 && is_leap(year));
        mon++;
    }
    td->year = year;
    td->mon = mon;
    td->mday = days + 1;
}

/* ---- alarms ---- */

static OSAlarm* alarm_head;

void OSInitAlarm(void) {}

void OSCreateAlarm(OSAlarm* alarm)
{
    memset(alarm, 0, sizeof(*alarm));
}

static void alarm_unlink(OSAlarm* alarm)
{
    OSAlarm** pp = &alarm_head;
    while (*pp) {
        if (*pp == alarm) {
            *pp = alarm->next;
            alarm->next = NULL;
            alarm->handler = NULL;
            return;
        }
        pp = &(*pp)->next;
    }
}

static void alarm_insert(OSAlarm* alarm, OSTime fire, OSAlarmHandler handler)
{
    alarm_unlink(alarm);
    alarm->handler = handler;
    alarm->fire = fire;
    alarm->next = alarm_head;
    alarm_head = alarm;
}

void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler)
{
    alarm->period = 0;
    alarm_insert(alarm, port_ticks + tick, handler);
}

void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler)
{
    alarm->period = 0;
    alarm_insert(alarm, time, handler);
}

void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period,
                        OSAlarmHandler handler)
{
    alarm->period = period;
    alarm->start = start;
    alarm_insert(alarm, start, handler);
}

void OSCancelAlarm(OSAlarm* alarm)
{
    alarm_unlink(alarm);
}

static void run_alarms(void)
{
    OSAlarm* a;
    /* Handlers may set or cancel alarms; restart the scan after each. */
again:
    for (a = alarm_head; a; a = a->next) {
        if (a->fire <= port_ticks) {
            OSAlarmHandler handler = a->handler;
            if (a->period) {
                a->fire += a->period;
            } else {
                alarm_unlink(a);
            }
            if (handler) {
                handler(a, NULL);
            }
            goto again;
        }
    }
}

void port_os_retrace(void)
{
    port_ticks += PORT_TICKS_PER_RETRACE;
    flush_deferred();
    run_alarms();
}

/* ---- interrupts: enable state + deferred "interrupt" callbacks ---- */

static BOOL interrupts_enabled = TRUE;

#define DEFER_MAX 256

static struct {
    void (*fn)(void*);
    void* arg;
} defer_queue[DEFER_MAX];
static unsigned defer_head, defer_tail;
static int in_deferred;

void port_defer(void (*fn)(void*), void* arg)
{
    unsigned next = (defer_tail + 1) % DEFER_MAX;
    if (next == defer_head) {
        port_log("port_defer: queue full, dropping callback");
        return;
    }
    defer_queue[defer_tail].fn = fn;
    defer_queue[defer_tail].arg = arg;
    defer_tail = next;
}

/// Run queued callbacks as an interrupt handler would: with interrupts
/// masked, and never re-entrantly.
static void flush_deferred(void)
{
    if (in_deferred || !interrupts_enabled) {
        return;
    }
    in_deferred = 1;
    interrupts_enabled = FALSE;
    while (defer_head != defer_tail) {
        void (*fn)(void*) = defer_queue[defer_head].fn;
        void* arg = defer_queue[defer_head].arg;
        defer_head = (defer_head + 1) % DEFER_MAX;
        fn(arg);
    }
    interrupts_enabled = TRUE;
    in_deferred = 0;
}

BOOL OSDisableInterrupts(void)
{
    BOOL old = interrupts_enabled;
    interrupts_enabled = FALSE;
    return old;
}

BOOL OSRestoreInterrupts(BOOL level)
{
    BOOL old = interrupts_enabled;
    interrupts_enabled = level;
    if (level) {
        flush_deferred();
    }
    return old;
}

BOOL OSEnableInterrupts(void)
{
    return OSRestoreInterrupts(TRUE);
}

/* ---- contexts / threads: single-threaded, no-ops ---- */

u32 PPCMfmsr(void)
{
    return 0x2000; /* MSR[FP] */
}

void PPCMtmsr(u32 newMSR)
{
    (void) newMSR;
}

static OSContext current_context;

u32 OSSaveContext(OSContext* context)
{
    (void) context;
    return 0;
}

void OSClearContext(OSContext* context)
{
    memset(context, 0, sizeof(*context));
}

OSContext* OSGetCurrentContext(void)
{
    return &current_context;
}

void OSSetCurrentContext(OSContext* context)
{
    (void) context;
}

void OSLoadFPUContext(OSContext* fpuContext)
{
    (void) fpuContext;
}

void OSSaveFPUContext(OSContext* fpuContext)
{
    (void) fpuContext;
}

long OSCheckActiveThreads(void)
{
    return 1;
}

/* ---- misc system state ---- */

static u32 progressive_mode;
static u32 sound_mode = 1; /* stereo */

u32 OSGetProgressiveMode(void)
{
    return progressive_mode;
}

void OSSetProgressiveMode(u32 mode)
{
    progressive_mode = mode;
}

u32 OSGetSoundMode(void)
{
    return sound_mode;
}

void OSSetSoundMode(u32 mode)
{
    sound_mode = mode;
}

unsigned long OSGetResetCode(void)
{
    return 0;
}

BOOL OSGetResetSwitchState(void)
{
    return FALSE;
}

void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu)
{
    port_log("OSResetSystem(%d, 0x%x, %d): exiting", reset, resetCode,
             forceMenu);
    exit(0);
}

/* ---- init ---- */

void OSInit(void)
{
    static int done;
    if (done) {
        return;
    }
    done = 1;
    map_mem1();
    init_time();
    OSSetArenaLo((void*) PORT_ARENA_LO);
    OSSetArenaHi((void*) (PORT_MEM1_BASE + PORT_MEM1_SIZE));
    port_log("MEM1 mapped at 0x%08x, arena 0x%08x-0x%08x", PORT_MEM1_BASE,
             (u32) OSGetArenaLo(), (u32) OSGetArenaHi());
}
