/// Game-specific DAT root types (fighters, stages, items, menus), keyed by
/// public symbol name. Grows as the port loads more of the game.

#include <string.h>

#include <port/endian.h>
#include <port/port.h>
#include <port/swap.h>

int port_swap_game_public(const char* symbol, void* addr)
{
    (void) symbol;
    (void) addr;
    return 0;
}
