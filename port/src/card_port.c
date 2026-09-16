/// CARD (memory card) for the port: no card in either slot.
///
/// A generated stub would return 0 == CARD_RESULT_READY and make the game
/// believe a card mounted, so every entry point answers NOCARD instead.
/// Async calls never invoke their callback (nothing is in flight).

#include <dolphin/card.h>

void CARDInit(void) {}

BOOL CARDProbe(long chan)
{
    (void) chan;
    return FALSE;
}

s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize)
{
    (void) chan;
    if (memSize) {
        *memSize = 0;
    }
    if (sectorSize) {
        *sectorSize = 0;
    }
    return CARD_RESULT_NOCARD;
}

s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback,
                   CARDCallback attachCallback)
{
    (void) chan;
    (void) workArea;
    (void) detachCallback;
    (void) attachCallback;
    return CARD_RESULT_NOCARD;
}

s32 CARDUnmount(s32 chan)
{
    (void) chan;
    return CARD_RESULT_NOCARD;
}

s32 CARDCheckAsync(s32 chan, CARDCallback callback)
{
    (void) chan;
    (void) callback;
    return CARD_RESULT_NOCARD;
}

s32 CARDFormatAsync(s32 chan, CARDCallback callback)
{
    (void) chan;
    (void) callback;
    return CARD_RESULT_NOCARD;
}

s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed)
{
    (void) chan;
    if (byteNotUsed) {
        *byteNotUsed = 0;
    }
    if (filesNotUsed) {
        *filesNotUsed = 0;
    }
    return CARD_RESULT_NOCARD;
}

s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo)
{
    (void) chan;
    (void) fileName;
    (void) fileInfo;
    return CARD_RESULT_NOCARD;
}

s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo* fileInfo)
{
    (void) chan;
    (void) fileNo;
    (void) fileInfo;
    return CARD_RESULT_NOCARD;
}

s32 CARDClose(CARDFileInfo* fileInfo)
{
    (void) fileInfo;
    return CARD_RESULT_NOCARD;
}

s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size,
                    CARDFileInfo* fileInfo, CARDCallback callback)
{
    (void) chan;
    (void) fileName;
    (void) size;
    (void) fileInfo;
    (void) callback;
    return CARD_RESULT_NOCARD;
}

s32 CARDDeleteAsync(s32 chan, char* fileName, CARDCallback callback)
{
    (void) chan;
    (void) fileName;
    (void) callback;
    return CARD_RESULT_NOCARD;
}

s32 CARDRenameAsync(s32 chan, const char* oldName, const char* newName,
                    CARDCallback callback)
{
    (void) chan;
    (void) oldName;
    (void) newName;
    (void) callback;
    return CARD_RESULT_NOCARD;
}

s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset,
                  CARDCallback callback)
{
    (void) fileInfo;
    (void) buf;
    (void) length;
    (void) offset;
    (void) callback;
    return CARD_RESULT_NOCARD;
}

s32 CARDWriteAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset,
                   CARDCallback callback)
{
    (void) fileInfo;
    (void) buf;
    (void) length;
    (void) offset;
    (void) callback;
    return CARD_RESULT_NOCARD;
}

s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat)
{
    (void) chan;
    (void) fileNo;
    (void) stat;
    return CARD_RESULT_NOCARD;
}

s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat* stat,
                       CARDCallback callback)
{
    (void) chan;
    (void) fileNo;
    (void) stat;
    (void) callback;
    return CARD_RESULT_NOCARD;
}

long CARDGetXferredBytes(long chan)
{
    (void) chan;
    return 0;
}
