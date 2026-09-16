/// Crash reporter: there is no debugger on the build host, so on a fatal
/// signal print the faulting address, EIP and an EBP-chain backtrace
/// (the port is compiled with -fno-omit-frame-pointer). Resolve the
/// addresses with:  .venv/bin/python port/tools/symbolize.py port/build/melee <addr>...

/* Host-only file: drop the PPC intptr_t defines the game build passes
 * (musl's own typedefs would otherwise collide) and expose REG_EIP & co. */
#undef intptr_t
#undef uintptr_t
#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include <port/port.h>

static char alt_stack[64 * 1024];

static int readable(const void* p)
{
    /* Cheap sanity check: the only mappings we care about are the ELF
     * image (low), MEM1 (0x80000000..) and the stacks (high). */
    unsigned long a = (unsigned long) p;
    return a >= 0x1000 && a < 0xFFFFF000u;
}

static void on_fatal(int sig, siginfo_t* si, void* ctx)
{
    ucontext_t* uc = (ucontext_t*) ctx;
    unsigned long eip = uc->uc_mcontext.gregs[REG_EIP];
    unsigned long ebp = uc->uc_mcontext.gregs[REG_EBP];
    unsigned long esp = uc->uc_mcontext.gregs[REG_ESP];
    int i;

    fprintf(stderr, "\n[port] fatal signal %d (%s) addr=%p eip=0x%08lx esp=0x%08lx\n",
            sig, strsignal(sig), si->si_addr, eip, esp);
    /* After a call through a bad function pointer, eip is garbage but the
     * return address is still on top of the stack. */
    if (readable((void*) esp)) {
        fprintf(stderr, "[port] [esp]=0x%08lx (return address if eip is a bad call target)\n",
                *(unsigned long*) esp);
    }
    fprintf(stderr, "[port] backtrace: 0x%08lx", eip);
    for (i = 0; i < 48 && readable((void*) ebp) && (ebp & 3) == 0; i++) {
        unsigned long* frame = (unsigned long*) ebp;
        unsigned long ret = frame[1];
        if (!readable((void*) ret)) {
            break;
        }
        fprintf(stderr, " 0x%08lx", ret);
        if (frame[0] <= ebp) {
            break;
        }
        ebp = frame[0];
    }
    fprintf(stderr, "\n[port] symbolize: .venv/bin/python port/tools/symbolize.py port/build/melee <addrs>\n");
    _exit(128 + sig);
}

__attribute__((constructor)) static void install_crash_handler(void)
{
    stack_t ss;
    struct sigaction sa;
    int sigs[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
    unsigned i;

    ss.ss_sp = alt_stack;
    ss.ss_size = sizeof(alt_stack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_fatal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) {
        sigaction(sigs[i], &sa, NULL);
    }
}
