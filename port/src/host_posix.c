/// Host services for Linux (static musl). See port/include/port/host.h.

/* Host-only file: drop the PPC intptr_t defines the game build passes. */
#undef intptr_t
#undef uintptr_t
#define _GNU_SOURCE

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <port/host.h>

/* Static linking: code, data and bss sit between these. */
extern const char __ehdr_start[];
extern char _end[];

int port_host_map_fixed(u32 addr, size_t size)
{
    void* want = (void*) (unsigned long) addr;
    void* got = mmap(want, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    return got == want;
}

int port_host_in_image(u32 addr)
{
    return addr >= (unsigned long) __ehdr_start && addr < (unsigned long) _end;
}

int port_host_file_open(const char* path)
{
    return open(path, O_RDONLY);
}

long port_host_file_read_at(int handle, void* dst, u32 len, u64 off)
{
    return (long) pread(handle, dst, len, (off_t) off);
}
