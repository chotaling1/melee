#ifndef PORT_ENDIAN_H
#define PORT_ENDIAN_H

/// Big-endian DAT data on a little-endian host.
///
/// Game data (HSD archives) is stored big-endian. The port converts it in
/// place, once, in two layers:
///   1. port_archive_swap(): the archive header, relocation/public/extern
///      tables and every relocated pointer word. The relocation table lists
///      exactly where the pointers are, so this layer needs no types.
///   2. Typed swaps (port_swap32/16/f32 and the per-struct helpers) at the
///      points where code first interprets data with a known C type.
///
/// Every swapped byte range is recorded in a per-archive bitmap, so a
/// descriptor reachable from several places is only ever swapped once, and
/// pointers that do not point into a registered archive (host static data,
/// already-native heap objects) are left alone.

#include <dolphin/types.h>

#include <stddef.h>

/// Called at the top of HSD_ArchiveParse with the raw file buffer. Converts
/// the header, tables and pointer words to host order and registers the
/// buffer. Returns 1 if the buffer was converted (or already registered).
int port_archive_swap(u8* src, size_t file_size);

/// Forget an archive buffer (called when its memory is freed/reused).
void port_archive_forget(u8* src);

/// Swap a field in place if it lies inside a registered archive and has not
/// been swapped yet. Safe to call repeatedly on the same address.
void port_swap16(void* p);
void port_swap32(void* p);
static inline void port_swapf32(void* p)
{
    port_swap32(p);
}

/// Claim [p, p+n) for in-place conversion: returns 1 (and marks it) if the
/// range is inside a registered archive and not yet converted, else 0.
/// For conversions that are not plain byte swaps (e.g. bitfield units).
int port_claim(void* p, size_t n);

/// Swap `count` consecutive fields of the given width.
void port_swap16_array(void* p, size_t count);
void port_swap32_array(void* p, size_t count);

/// Size of the object starting at p: bytes up to the next address that any
/// relocated pointer or public symbol refers to (or the end of the data
/// section). 0 if p is not inside a registered archive's data.
size_t port_extent(const void* p);

/// Swap every 4-byte word of the object at p (port_extent). Relocated
/// pointer words are already claimed and are skipped, so this is right for
/// any object made only of pointers, f32 and 32-bit integers.
void port_swap32_extent(void* p);

/// Log objects in live archives that were never converted (see endian_port.c).
void port_swap_audit(void);

/// 1 if the 4-byte word at p is a relocated pointer (word-aligned in the
/// data section).
int port_is_pointer_word(const void* p);

/// 1 if the byte at p has already been converted (or claimed).
int port_is_claimed(const void* p);

/// Returns 1 if p points into a registered archive's data.
int port_in_archive(const void* p);

#endif
