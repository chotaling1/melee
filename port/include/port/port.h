#ifndef PORT_PORT_H
#define PORT_PORT_H

/// Internal interface between the port's platform-layer files (port/src).
/// Game code never includes this.

#include <dolphin/types.h>

/// Console MEM1: 24 MB mapped at its GameCube virtual address so that
/// pointers compare and print like they do on hardware / in Dolphin, and so
/// code that reads OS globals through fixed addresses (OS_BUS_CLOCK at
/// 0x800000F8, ...) keeps working.
#define PORT_MEM1_BASE 0x80000000u
#define PORT_MEM1_SIZE 0x01800000u

/// OS timebase: 162 MHz bus clock / 4.
#define PORT_TIMER_CLOCK 40500000u
/// Ticks per NTSC field (59.94 Hz).
#define PORT_TICKS_PER_RETRACE 675675u

/// Advance virtual time and deliver any interrupts that became due
/// (deferred device callbacks, OS alarms, VI retrace).
void port_advance(s64 ticks);

/// Advance virtual time until the next VI retrace has been delivered.
void port_wait_retrace(void);

/// The VI retrace interrupt handler (vi_port.c); called by the OS layer
/// with interrupts masked.
void port_vi_interrupt(void);

/// Queue a completion callback that on hardware would arrive from an
/// interrupt (ARQ/DVD/AX done). It runs the next time interrupts are
/// enabled (OSRestoreInterrupts), at a retrace, or on a timebase read, so
/// callers that post a request while interrupts are disabled and then keep
/// touching state (devcom.c does) behave as on the console.
void port_defer(void (*fn)(void*), void* arg);

/// Log helper for the platform layer (goes to stderr, prefixed).
void port_log(const char* fmt, ...);

/// Convert a main-memory address as passed to DMA APIs (either a cached
/// pointer or a physical address) into a host pointer.
void* port_mem1_ptr(u32 addr);

#endif
