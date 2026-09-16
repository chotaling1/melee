/// Host services for Windows (Zig x86-windows-gnu). See
/// port/include/port/host.h.
///
/// MEM1 lives at 0x80000000 and the hardware register page at 0xCC000000,
/// above 2 GB, so the executable is linked large-address-aware; a 32-bit
/// large-address-aware process on 64-bit Windows gets a 4 GB user address
/// space.

#undef intptr_t
#undef uintptr_t
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <port/host.h>

#define MAX_FILES 8

static HANDLE files[MAX_FILES];

/* Provided by the MinGW-style linker: the image's DOS header. */
extern IMAGE_DOS_HEADER __ImageBase;

int port_host_map_fixed(u32 addr, size_t size)
{
    LPVOID want = (LPVOID) (ULONG_PTR) addr;
    LPVOID got = VirtualAlloc(want, size, MEM_RESERVE | MEM_COMMIT,
                              PAGE_READWRITE);
    return got == want;
}

int port_host_in_image(u32 addr)
{
    ULONG_PTR base = (ULONG_PTR) &__ImageBase;
    IMAGE_NT_HEADERS* nt =
        (IMAGE_NT_HEADERS*) ((BYTE*) &__ImageBase + __ImageBase.e_lfanew);
    return addr >= base && addr < base + nt->OptionalHeader.SizeOfImage;
}

int port_host_file_open(const char* path)
{
    int i;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }
    for (i = 0; i < MAX_FILES; i++) {
        if (files[i] == NULL) {
            files[i] = h;
            return i;
        }
    }
    CloseHandle(h);
    return -1;
}

long port_host_file_read_at(int handle, void* dst, u32 len, u64 off)
{
    OVERLAPPED ov;
    DWORD got = 0;
    if (handle < 0 || handle >= MAX_FILES || files[handle] == NULL) {
        return -1;
    }
    ZeroMemory(&ov, sizeof(ov));
    ov.Offset = (DWORD) off;
    ov.OffsetHigh = (DWORD) (off >> 32);
    if (!ReadFile(files[handle], dst, len, &got, &ov)) {
        return GetLastError() == ERROR_HANDLE_EOF ? 0 : -1;
    }
    return (long) got;
}
