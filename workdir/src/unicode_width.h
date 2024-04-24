#ifndef UNICODE_WIDTH_H
#define UNICODE_WIDTH_H
#include <windows.h>

BOOL is_wide(UINT32 c);

#define UNICODE_WIDTH(c) (is_wide(c) ? 2 : 1)

#define UNICODE_PAIR_WIDTH(hs, ls) (UNICODE_WIDTH(((hs & 0x3FF) << 10) | (ls & 0x3FF)))

#endif
