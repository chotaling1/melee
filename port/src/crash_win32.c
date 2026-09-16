/// Crash reporter for Windows: same output as crash_posix.c (faulting
/// address, EIP, EBP-chain backtrace) from an unhandled exception filter.
/// Symbolize with the PDB-less build's map: port/tools/symbolize.py works on
/// the ELF build; on Windows, compare addresses against melee.exe's image
/// base printed here.

#undef intptr_t
#undef uintptr_t
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

extern IMAGE_DOS_HEADER __ImageBase;

static int readable(ULONG_PTR a)
{
    return a >= 0x10000 && a < 0xFFFE0000u;
}

static LONG WINAPI on_fatal(EXCEPTION_POINTERS* ep)
{
    CONTEXT* c = ep->ContextRecord;
    EXCEPTION_RECORD* r = ep->ExceptionRecord;
    ULONG_PTR ebp = c->Ebp;
    int i;

    fprintf(stderr,
            "\n[port] fatal exception 0x%08lx addr=%p eip=0x%08lx "
            "esp=0x%08lx image base=%p\n",
            (unsigned long) r->ExceptionCode,
            r->NumberParameters >= 2 ? (void*) r->ExceptionInformation[1]
                                     : NULL,
            (unsigned long) c->Eip, (unsigned long) c->Esp,
            (void*) &__ImageBase);
    if (readable(c->Esp) && !IsBadReadPtr((void*) c->Esp, 4)) {
        fprintf(stderr, "[port] [esp]=0x%08lx\n",
                *(unsigned long*) c->Esp);
    }
    fprintf(stderr, "[port] backtrace: 0x%08lx", (unsigned long) c->Eip);
    for (i = 0; i < 48 && readable(ebp) && (ebp & 3) == 0 &&
                !IsBadReadPtr((void*) ebp, 8);
         i++)
    {
        ULONG_PTR* frame = (ULONG_PTR*) ebp;
        if (!readable(frame[1])) {
            break;
        }
        fprintf(stderr, " 0x%08lx", (unsigned long) frame[1]);
        if (frame[0] <= ebp) {
            break;
        }
        ebp = frame[0];
    }
    fprintf(stderr, "\n");
    fflush(stderr);
    ExitProcess(128 + 11);
    return EXCEPTION_EXECUTE_HANDLER;
}

__attribute__((constructor)) static void install_crash_handler(void)
{
    SetUnhandledExceptionFilter(on_fatal);
}
