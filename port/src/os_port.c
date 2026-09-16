/// OS layer for the port: memory map, console output, time, alarms.
///
/// Heaps (OSAlloc.c) and the arena bump allocator (OSArena.c) are the
/// SDK's own C code, compiled straight from libs/dolphin.

#include <dolphin/os.h>
#include <dolphin/vi.h>

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

    /* Hardware registers (0xCC000000..). The SDK's inline GX macros
     * (GXWGFifo) write command bytes straight to 0xCC008000. Headless, a
     * RAM page there turns those writes into no-ops; the renderer will
     * replace the macros instead. */
    got = mmap((void*) 0xCC000000u, 0x10000, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (got != (void*) 0xCC000000u) {
        port_log("cannot map hardware registers at 0xCC000000");
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
static OSTime next_retrace;

static void init_time(void)
{
    /* Deterministic by default: the RNG is seeded from OSGetTick() in
     * main(), so a fixed start time makes every headless run identical.
     * MELEE_PORT_CLOCK=wall starts from the real date instead.
     * GameCube epoch is 2000-01-01 00:00:00. */
    const char* clock = getenv("MELEE_PORT_CLOCK");
    time_t start = 1007337600; /* 2001-12-03 00:00:00 UTC, NA launch */
    if (clock != NULL && strcmp(clock, "wall") == 0) {
        start = time(NULL);
    }
    port_ticks = (OSTime) (start - 946684800) * PORT_TIMER_CLOCK;
    next_retrace = port_ticks + PORT_TICKS_PER_RETRACE;
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
    OSTime fire = start;
    alarm->period = period;
    alarm->start = start;
    /* As in the SDK, `start` is absolute and may be far in the past: the
     * first firing is the next period boundary after now. */
    if (period > 0 && fire <= port_ticks) {
        fire = start + ((port_ticks - start) / period + 1) * period;
    }
    alarm_insert(alarm, fire, handler);
}

void OSCancelAlarm(OSAlarm* alarm)
{
    alarm_unlink(alarm);
}

/// Fire the earliest due alarm, if any. Returns 1 if one fired.
static int run_one_alarm(void)
{
    OSAlarm* a;
    OSAlarm* due = NULL;
    OSAlarmHandler handler;

    for (a = alarm_head; a; a = a->next) {
        if (a->fire <= port_ticks && (!due || a->fire < due->fire)) {
            due = a;
        }
    }
    if (!due) {
        return 0;
    }
    handler = due->handler;
    if (due->period) {
        due->fire += due->period;
    } else {
        alarm_unlink(due);
    }
    if (handler) {
        handler(due, NULL);
    }
    return 1;
}

/* ---- interrupts ----
 *
 * Time model: on the console, alarm (decrementer) and VI (retrace)
 * interrupts arrive asynchronously. Here they are delivered at
 * "interrupt windows": whenever interrupts become enabled, and whenever
 * the timebase is read. Each window also advances virtual time a little,
 * so busy-wait loops (e.g. gm_801A4D34 waiting for the 60 Hz pad alarm)
 * make progress. All of this is deterministic: the same code path always
 * sees the same interrupt timing.
 */

/// Virtual time consumed per interrupt-enable window (~50 us).
#define WINDOW_TICKS 2025

static BOOL interrupts_enabled = TRUE;
static int in_irq;

#define DEFER_MAX 256

static struct {
    void (*fn)(void*);
    void* arg;
} defer_queue[DEFER_MAX];
static unsigned defer_head, defer_tail;

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

/// Deliver everything that is due, as interrupt handlers would: with
/// interrupts masked and never re-entrantly. Order per pass: device
/// completions, then alarms, then the VI retrace.
static void service_interrupts(void)
{
    int progress;

    if (in_irq || !interrupts_enabled) {
        return;
    }
    in_irq = 1;
    interrupts_enabled = FALSE;
    do {
        progress = 0;
        while (defer_head != defer_tail) {
            void (*fn)(void*) = defer_queue[defer_head].fn;
            void* arg = defer_queue[defer_head].arg;
            defer_head = (defer_head + 1) % DEFER_MAX;
            fn(arg);
            progress = 1;
        }
        if (run_one_alarm()) {
            progress = 1;
        }
        if (port_ticks >= next_retrace) {
            next_retrace += PORT_TICKS_PER_RETRACE;
            port_vi_interrupt();
            progress = 1;
        }
    } while (progress);
    interrupts_enabled = TRUE;
    in_irq = 0;
}

static void flush_deferred(void)
{
    service_interrupts();
}

void port_advance(OSTime ticks)
{
    port_ticks += ticks;
    service_interrupts();
}

void port_wait_retrace(void)
{
    u32 start = VIGetRetraceCount();
    while (VIGetRetraceCount() == start) {
        OSTime dt = next_retrace - port_ticks;
        port_advance(dt > 0 ? dt : 1);
    }
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
    if (level && !old) {
        port_advance(WINDOW_TICKS);
    } else if (level) {
        service_interrupts();
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
