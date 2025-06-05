#ifndef UNICODE_WIDTH_H
#define UNICODE_WIDTH_H
#include <stdint.h>
#include <stdbool.h>

bool unicode_is_wide(uint32_t c);

uint32_t unicode_case_fold_utf32(uint32_t c);

uint32_t unicode_case_fold_utf8(const uint8_t* in, uint32_t len, uint8_t* out);

extern const uint8_t utf8_len_table[256];

extern const uint8_t utf8_valid_table[128];

#define UNICODE_WIDTH(c) (unicode_is_wide(c) ? 2 : 1)

#define UNICODE_PAIR_WIDTH(hs, ls) (UNICODE_WIDTH(((hs & 0x3FF) << 10) | (ls & 0x3FF)))

#define CAN_START_SEQ(c) ((c) <= 127 || utf8_valid_table[(c) - 128] != 0)

uint8_t get_utf8_seq(const uint8_t* bytes, uint64_t len);

#endif
