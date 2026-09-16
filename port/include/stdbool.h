#ifndef PORT_STDBOOL_H
#define PORT_STDBOOL_H

/// Port override of <stdbool.h>. The matching build (MWCC via src/MSL)
/// defines bool as a 4-byte int, and the game's structs and DAT layouts
/// depend on that. Clang's own stdbool.h would make it a 1-byte _Bool and
/// also makes `bool` vs `int` prototypes incompatible. This header wins
/// because -Iport/include is searched before the compiler's system dirs.
typedef int bool;

#define true 1
#define false 0

#endif
