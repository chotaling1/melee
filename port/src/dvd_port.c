/// DVD for the port: reads straight from the user's own GameCube ISO.
///
/// The disc image path comes from $MELEE_ISO, defaulting to the dump in
/// orig/GALE01/. Nothing from the disc is ever written anywhere; the FST
/// is parsed in memory and file reads are positioned host reads at the file's disc
/// offset. Completion callbacks are delivered through port_defer, like the
/// DVD interrupt would on hardware.
///
/// Disc layout (all big-endian):
///   0x0000  DVDDiskID (game "GALE", maker "01", disc #, version)
///   0x0424  FST offset, 0x0428 FST size
///   FST: 12-byte entries {flags:8 name:24, offset|parent, length|next},
///        entry 0 is the root and its `next` is the entry count; the
///        string table follows the entries.

#undef intptr_t
#undef uintptr_t

#include <dolphin/dvd.h>

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <port/host.h>
#include <port/port.h>

#define DEFAULT_ISO                                                           \
    "orig/GALE01/Super Smash Bros. Melee (USA) (En,Ja) (v1.02).iso"

#define FST_ENTRY_SIZE 12

static int iso_fd = -1;
static DVDDiskID disk_id;
static u8* fst;
static u32 fst_entries;
static const char* fst_strings;
static int trace;

static u32 be32(const u8* p)
{
    return ((u32) p[0] << 24) | ((u32) p[1] << 16) | ((u32) p[2] << 8) | p[3];
}

static int read_at(void* dst, u32 len, u64 off)
{
    u8* p = dst;
    while (len) {
        long n = port_host_file_read_at(iso_fd, p, len, off);
        if (n <= 0) {
            return 0;
        }
        p += n;
        len -= (u32) n;
        off += (u64) n;
    }
    return 1;
}

/* FST entry accessors */
static int entry_is_dir(u32 i)
{
    return fst[i * FST_ENTRY_SIZE] != 0;
}

static const char* entry_name(u32 i)
{
    u32 off = be32(fst + i * FST_ENTRY_SIZE) & 0x00FFFFFF;
    return fst_strings + off;
}

static u32 entry_offset(u32 i)
{
    return be32(fst + i * FST_ENTRY_SIZE + 4);
}

static u32 entry_length(u32 i)
{
    return be32(fst + i * FST_ENTRY_SIZE + 8);
}

/* For directories, index of the first entry after the directory. */
#define entry_next entry_length

void DVDInit(void)
{
    const char* path;
    u8 header[0x440];
    u32 fst_off, fst_size;

    if (iso_fd >= 0) {
        return;
    }
    trace = getenv("MELEE_PORT_TRACE_DVD") != NULL;
    path = getenv("MELEE_ISO");
    if (!path) {
        path = DEFAULT_ISO;
    }
    iso_fd = port_host_file_open(path);
    if (iso_fd < 0) {
        port_log("cannot open ISO '%s' (set MELEE_ISO)", path);
        abort();
    }
    if (!read_at(header, sizeof(header), 0)) {
        port_log("cannot read ISO header");
        abort();
    }
    memcpy(&disk_id, header, sizeof(disk_id));
    fst_off = be32(header + 0x424);
    fst_size = be32(header + 0x428);
    fst = malloc(fst_size);
    if (!fst || !read_at(fst, fst_size, fst_off)) {
        port_log("cannot read FST (offset 0x%x, size 0x%x)", fst_off,
                 fst_size);
        abort();
    }
    fst_entries = entry_next(0);
    fst_strings = (const char*) (fst + fst_entries * FST_ENTRY_SIZE);
    port_log("disc %.4s%.2s v1.%02d: %u FST entries", disk_id.gameName,
             disk_id.company, disk_id.gameVersion, fst_entries);
}

s32 DVDConvertPathToEntrynum(const char* pathPtr)
{
    const char* p = pathPtr;
    u32 dir = 0;

    if (iso_fd < 0) {
        DVDInit();
    }
    while (*p == '/') {
        p++;
    }
    while (*p) {
        const char* end = p;
        size_t len;
        u32 i, found = 0;
        int matched = 0;

        while (*end && *end != '/') {
            end++;
        }
        len = (size_t) (end - p);
        if (len == 1 && p[0] == '.') {
            /* stay in dir */
            matched = 1;
            found = dir;
        } else if (len == 2 && p[0] == '.' && p[1] == '.') {
            found = entry_offset(dir); /* parent */
            matched = 1;
        } else {
            for (i = dir + 1; i < entry_next(dir);) {
                const char* name = entry_name(i);
                if (strlen(name) == len && strncasecmp(name, p, len) == 0) {
                    found = i;
                    matched = 1;
                    break;
                }
                i = entry_is_dir(i) ? entry_next(i) : i + 1;
            }
        }
        if (!matched) {
            port_log("DVDConvertPathToEntrynum(\"%s\"): not found", pathPtr);
            return -1;
        }
        p = end;
        while (*p == '/') {
            p++;
        }
        if (*p) {
            if (!entry_is_dir(found)) {
                port_log("DVDConvertPathToEntrynum(\"%s\"): not a directory",
                         pathPtr);
                return -1;
            }
            dir = found;
        } else {
            if (trace) {
                port_log("DVD entry %u: %s (0x%x bytes at 0x%x)", found,
                         pathPtr, entry_length(found), entry_offset(found));
            }
            return (s32) found;
        }
    }
    return (s32) dir;
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo)
{
    if (entrynum < 0 || (u32) entrynum >= fst_entries ||
        entry_is_dir((u32) entrynum)) {
        return FALSE;
    }
    memset(fileInfo, 0, sizeof(*fileInfo));
    fileInfo->startAddr = entry_offset((u32) entrynum);
    fileInfo->length = entry_length((u32) entrynum);
    fileInfo->cb.state = DVD_STATE_END;
    return TRUE;
}

static void read_complete(void* arg)
{
    DVDFileInfo* fileInfo = arg;
    fileInfo->cb.state = DVD_STATE_END;
    if (fileInfo->callback) {
        fileInfo->callback((s32) fileInfo->cb.transferredSize, fileInfo);
    }
}

BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length,
                      s32 offset, DVDCallback callback, s32 prio)
{
    (void) prio;
    if (trace) {
        port_log("DVD read 0x%x bytes at +0x%x of file @0x%x -> %p", length,
                 offset, fileInfo->startAddr, addr);
    }
    if (!read_at(addr, (u32) length, (u64) fileInfo->startAddr + (u32) offset)) {
        port_log("DVD read failed (0x%x bytes at +0x%x of file @0x%x)", length,
                 offset, fileInfo->startAddr);
        return FALSE;
    }
    fileInfo->callback = callback;
    fileInfo->cb.addr = addr;
    fileInfo->cb.offset = (u32) offset;
    fileInfo->cb.length = (u32) length;
    fileInfo->cb.transferredSize = (u32) length;
    if (callback == NULL) {
        /* Nothing to notify: finish now rather than touching fileInfo
         * later, since callers often keep it on the stack. */
        fileInfo->cb.state = DVD_STATE_END;
        return TRUE;
    }
    fileInfo->cb.state = DVD_STATE_BUSY;
    port_defer(read_complete, fileInfo);
    return TRUE;
}

BOOL DVDClose(DVDFileInfo* fileInfo)
{
    (void) fileInfo;
    return TRUE;
}

BOOL DVDCheckDisk(void)
{
    return TRUE;
}

long DVDGetDriveStatus(void)
{
    return DVD_STATE_END;
}

DVDDiskID* DVDGetCurrentDiskID(void)
{
    if (iso_fd < 0) {
        DVDInit();
    }
    return &disk_id;
}
