/// AR / ARQ (auxiliary RAM) for the port: 16 MB of host memory.
///
/// ARAM addresses are byte offsets into that buffer, exactly like the
/// hardware's 0..0x1000000 address space. The SDK reserves the first
/// 0x4000 bytes for the DSP, so ARInit's stack starts there. ARQ "DMA" is
/// a synchronous memcpy followed by the completion callback.

#include <dolphin/ar.h>

#include <string.h>

#include <port/port.h>

#define ARAM_SIZE 0x01000000u
#define ARAM_STACK_BASE 0x4000u

static u8 aram[ARAM_SIZE] __attribute__((aligned(32)));
static u32* ar_stack;
static u32 ar_stack_max;
static u32 ar_stack_top;
static u32 ar_stack_ptr;
static int ar_initialized;

u32 ARInit(u32* stack_index_addr, u32 num_entries)
{
    if (ar_initialized) {
        return ar_stack_ptr;
    }
    ar_stack = stack_index_addr;
    ar_stack_max = num_entries;
    ar_stack_top = 0;
    ar_stack_ptr = ARAM_STACK_BASE;
    ar_initialized = 1;
    return ar_stack_ptr;
}

int ARCheckInit(void)
{
    return ar_initialized;
}

u32 ARAlloc(u32 length)
{
    u32 addr = ar_stack_ptr;
    if (ar_stack_top >= ar_stack_max || addr + length > ARAM_SIZE) {
        port_log("ARAlloc(%u): out of ARAM stack", length);
        return 0;
    }
    ar_stack[ar_stack_top++] = length;
    ar_stack_ptr += length;
    return addr;
}

u32 ARFree(u32* length)
{
    u32 len = ar_stack[--ar_stack_top];
    ar_stack_ptr -= len;
    if (length) {
        *length = len;
    }
    return ar_stack_ptr;
}

u32 ARGetSize(void)
{
    return ARAM_SIZE;
}

u32 ARGetBaseAddress(void)
{
    return ARAM_STACK_BASE;
}

u32 ARGetDMAStatus(void)
{
    return 0;
}

void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length)
{
    void* mem = port_mem1_ptr(mainmem_addr);
    if (aram_addr + length > ARAM_SIZE) {
        port_log("ARStartDMA: aram range 0x%x+0x%x out of bounds", aram_addr,
                 length);
        return;
    }
    if (type == ARAM_DIR_MRAM_TO_ARAM) {
        memcpy(aram + aram_addr, mem, length);
    } else {
        memcpy(mem, aram + aram_addr, length);
    }
}

void ARQInit(void) {}

static void arq_complete(void* arg)
{
    ARQRequest* request = arg;
    if (request->callback) {
        request->callback(request);
    }
}

/// The copy happens immediately; the completion callback is delivered like
/// the hardware's DMA-done interrupt would be (see port_defer).
void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority,
                    u32 source, u32 dest, u32 length, ARQCallback callback)
{
    (void) priority;
    request->owner = owner;
    request->type = type;
    request->source = source;
    request->dest = dest;
    request->length = length;
    request->callback = callback;
    if (type == ARQ_TYPE_MRAM_TO_ARAM) {
        ARStartDMA(ARAM_DIR_MRAM_TO_ARAM, source, dest, length);
    } else {
        ARStartDMA(ARAM_DIR_ARAM_TO_MRAM, dest, source, length);
    }
    port_defer(arq_complete, request);
}
