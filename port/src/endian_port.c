/// In-place big-endian -> host conversion of HSD archive data.
/// See port/include/port/endian.h for the model.

#include <stdlib.h>
#include <string.h>

#include <port/endian.h>
#include <port/port.h>

#define MAX_ARCHIVES 512
#define HEADER_SIZE 0x20

typedef struct {
    u8* base;      ///< start of the file buffer
    u32 size;      ///< file size
    u8* done;      ///< 1 bit per byte: already in host order
    u32 data_size; ///< size of the data section (starts at HEADER_SIZE)
    u32* starts;   ///< sorted data offsets that something points at
    u32 nstarts;
} Region;

static Region regions[MAX_ARCHIVES];
static int nregions;

static u32 bswap32(u32 v)
{
    return __builtin_bswap32(v);
}

static Region* find_region(const u8* p)
{
    int i;
    for (i = 0; i < nregions; i++) {
        Region* r = &regions[i];
        if (p >= r->base && p < r->base + r->size) {
            return r;
        }
    }
    return NULL;
}

int port_in_archive(const void* p)
{
    return find_region(p) != NULL;
}

/// Mark [off, off+len) swapped; returns 0 if any byte was already marked
/// (the caller then skips the swap).
static int claim(Region* r, u32 off, u32 len)
{
    u32 i;
    if (off + len > r->size) {
        return 0;
    }
    for (i = off; i < off + len; i++) {
        if (r->done[i >> 3] & (1 << (i & 7))) {
            return 0;
        }
    }
    for (i = off; i < off + len; i++) {
        r->done[i >> 3] |= (u8) (1 << (i & 7));
    }
    return 1;
}

void port_archive_forget(u8* src)
{
    int i;
    for (i = 0; i < nregions; i++) {
        if (regions[i].base == src) {
            free(regions[i].done);
            free(regions[i].starts);
            regions[i] = regions[--nregions];
            return;
        }
    }
}

static Region* add_region(u8* src, u32 size)
{
    Region* r;
    int i;

    /* A new file loaded over an old buffer: drop any overlapping regions. */
    for (i = 0; i < nregions;) {
        Region* o = &regions[i];
        if (o->base < src + size && src < o->base + o->size) {
            free(o->done);
            free(o->starts);
            regions[i] = regions[--nregions];
        } else {
            i++;
        }
    }
    if (nregions == MAX_ARCHIVES) {
        port_log("port_archive_swap: too many archives");
        abort();
    }
    r = &regions[nregions++];
    r->base = src;
    r->size = size;
    r->done = calloc((size + 7) / 8, 1);
    r->data_size = 0;
    r->starts = NULL;
    r->nstarts = 0;
    return r;
}

static void swap_words(Region* r, u32 off, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++, off += 4) {
        if (claim(r, off, 4)) {
            u32* w = (u32*) (r->base + off);
            *w = bswap32(*w);
        }
    }
}

static int cmp_u32(const void* a, const void* b)
{
    u32 x = *(const u32*) a, y = *(const u32*) b;
    return x < y ? -1 : x > y;
}

int port_archive_swap(u8* src, size_t file_size)
{
    u32* hdr = (u32*) src;
    u32 data_size, nb_reloc, nb_public, nb_extern;
    u32 data_off, reloc_off, public_off, extern_off;
    u32 i;
    Region* r;

    if (src == NULL) {
        return 0;
    }
    if (hdr[0] == file_size) {
        return 1; /* already converted (or a native archive) */
    }
    if (bswap32(hdr[0]) != file_size) {
        return 0; /* not an archive of this size */
    }
    /* Big-endian header: fresh file data, possibly loaded over a buffer that
     * held an earlier archive (fighter animations reuse one buffer), so the
     * old region is dropped by add_region. */

    r = add_region(src, (u32) file_size);

    swap_words(r, 0, 5); /* file_size..nb_extern; version/pad are bytes */
    claim(r, 0x14, 0x0C);

    data_size = hdr[1];
    nb_reloc = hdr[2];
    nb_public = hdr[3];
    nb_extern = hdr[4];

    data_off = HEADER_SIZE;
    reloc_off = HEADER_SIZE + data_size;
    public_off = reloc_off + nb_reloc * 4;
    extern_off = public_off + nb_public * 8;

    swap_words(r, reloc_off, nb_reloc);
    swap_words(r, public_off, nb_public * 2);
    swap_words(r, extern_off, nb_extern * 2);

    /* Every relocated word is a pointer (an offset from the data section,
     * turned into an address by HSD_ArchiveParse's Locate()). */
    for (i = 0; i < nb_reloc; i++) {
        u32 off = ((u32*) (src + reloc_off))[i];
        swap_words(r, data_off + off, 1);
    }

    /* External references are pointer slots too (patched by
     * HSD_ArchiveLocateExtern). */
    for (i = 0; i < nb_extern; i++) {
        u32 off = ((u32*) (src + extern_off))[i * 2];
        swap_words(r, data_off + off, 1);
    }

    /* Object starts: every pointer target and public symbol. They bound
     * the extent of each object (see port_extent). */
    r->data_size = data_size;
    r->starts = malloc((nb_reloc + nb_public + 1) * sizeof(u32));
    for (i = 0; i < nb_reloc; i++) {
        u32 off = ((u32*) (src + reloc_off))[i];
        r->starts[r->nstarts++] = *(u32*) (src + data_off + off);
    }
    for (i = 0; i < nb_public; i++) {
        r->starts[r->nstarts++] = ((u32*) (src + public_off))[i * 2];
    }
    qsort(r->starts, r->nstarts, sizeof(u32), cmp_u32);

    return 1;
}

int port_claim(void* p, size_t n)
{
    Region* r = find_region(p);
    return r != NULL && claim(r, (u32) ((u8*) p - r->base), (u32) n);
}

void port_swap32(void* p)
{
    Region* r = find_region(p);
    if (r && claim(r, (u32) ((u8*) p - r->base), 4)) {
        u32* w = p;
        *w = bswap32(*w);
    }
}

void port_swap16(void* p)
{
    Region* r = find_region(p);
    if (r && claim(r, (u32) ((u8*) p - r->base), 2)) {
        u16* w = p;
        *w = __builtin_bswap16(*w);
    }
}

void port_swap32_array(void* p, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++) {
        port_swap32((u8*) p + i * 4);
    }
}

void port_swap16_array(void* p, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++) {
        port_swap16((u8*) p + i * 2);
    }
}

size_t port_extent(const void* p)
{
    Region* r = find_region(p);
    u32 off, lo, hi;
    if (r == NULL || r->starts == NULL) {
        return 0;
    }
    off = (u32) ((u8*) p - r->base);
    if (off < HEADER_SIZE || off >= HEADER_SIZE + r->data_size) {
        return 0;
    }
    off -= HEADER_SIZE;
    /* First start > off. */
    lo = 0;
    hi = r->nstarts;
    while (lo < hi) {
        u32 mid = (lo + hi) / 2;
        if (r->starts[mid] <= off) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return (lo < r->nstarts ? r->starts[lo] : r->data_size) - off;
}

void port_swap32_extent(void* p)
{
    port_swap32_array(p, port_extent(p) / 4);
}
