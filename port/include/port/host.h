#ifndef PORT_HOST_H
#define PORT_HOST_H

/// Host operating system services used by the platform layer. One
/// implementation per host: port/src/host_posix.c (Linux) and
/// port/src/host_win32.c (Windows); port/configure.py picks by target.

#include <dolphin/types.h>

#include <stddef.h>

/// Map `size` bytes of zeroed read/write memory at exactly `addr`.
/// Returns 1 on success.
int port_host_map_fixed(u32 addr, size_t size);

/// 1 if `addr` lies in the executable's own image (code, data, bss).
int port_host_in_image(u32 addr);

/// Open a file read-only; returns a handle >= 0 or -1.
int port_host_file_open(const char* path);

/// Read up to `len` bytes at absolute offset `off`; returns the number of
/// bytes read (0 at end of file) or -1 on error.
long port_host_file_read_at(int handle, void* dst, u32 len, u64 off);

#endif
