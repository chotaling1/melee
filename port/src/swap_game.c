/// Game-specific DAT root types (fighters, stages, items, menus), keyed by
/// public symbol name. Grows as the port loads more of the game.

#include <string.h>

#include <port/endian.h>
#include <port/port.h>
#include <port/swap.h>

#define OK(p) ((p) != NULL && port_in_archive(p))

/// LbRf.dat "lbRefData" (src/melee/lb/lbrefract.c): { u8 count; f32* params }
/// with two f32 parameters per refraction texture.
static void swap_lbRefData(void* addr)
{
    struct {
        u8 count;
        f32* params;
    }* d = addr;
    if (OK(d->params)) {
        port_swap32_array(d->params, (size_t) d->count * 2);
    }
}

typedef struct {
    const char* symbol;
    void (*swap)(void* addr);
} GameRoot;

static const GameRoot roots[] = {
    { "lbRefData", swap_lbRefData },
};

int port_swap_game_public(const char* symbol, void* addr)
{
    size_t i;
    for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        if (strcmp(symbol, roots[i].symbol) == 0) {
            roots[i].swap(addr);
            return 1;
        }
    }
    return 0;
}
