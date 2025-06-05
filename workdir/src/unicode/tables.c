#include "unicode_utils.h"

const uint8_t utf8_len_table[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1
};

const uint8_t utf8_valid_table[128] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0xa0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x9f, 0x80, 0x80, 0x90, 0x80, 0x80, 0x80, 0x8f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// get how many bytes should be considered one sequence,
// the sequence might be invalid.
// If the sequence is invalid, includes byte 0 and up to
// 3 bytes that cannot start a sequence
uint8_t get_utf8_seq(const uint8_t* bytes, uint64_t len) {
    if (len < 2) {
        return len;
    }
    if (bytes[0] <= 127) {
        return 1;
    }
    uint8_t v = utf8_valid_table[bytes[0] - 128];
    uint8_t l = utf8_len_table[bytes[0]];
    if (v == 0) { // Illegal byte 0
        if (!CAN_START_SEQ(bytes[1])) {
            if (len > 2 && !CAN_START_SEQ(bytes[2])) {
                if (len > 3 && !CAN_START_SEQ(bytes[3])) {
                    return 4;
                }
                return 3;
            }
            return 2;
        }
        return 1;
    }

    if (l > len) { // Not enough bytes for full sequence
        if (!CAN_START_SEQ(bytes[1])) {
            if (len > 2 && !CAN_START_SEQ(bytes[2])) {
                return 3;
            }
            return 2;
        }
        return 1;
    }

    if (((v & 1) && (bytes[1] < 0x80 || bytes[1] > v)) ||
         (!(v & 1) && (bytes[1] < v || bytes[1] > 0xBF))) { // Illegal byte 1
        // Still include all bytes that cannot start a sequence
        if (!CAN_START_SEQ(bytes[1])) {
            if (len > 2 && !CAN_START_SEQ(bytes[2])) {
                if (len > 3 && !CAN_START_SEQ(bytes[3])) {
                    return 4;
                }
                return 3;
            }
            return 2;
        }
        return 1;
    }
    if (l > 2) {
        if (bytes[2] < 0x80 || bytes[2] > 0xBF) { // Illegal byte 2
            if (CAN_START_SEQ(bytes[2])) {
                return 2;
            }
            if (l > 3 && !CAN_START_SEQ(bytes[3])) {
                return 4;
            }
            return 3;
        }
        if (l > 3) {
            if (bytes[3] < 0x80 || bytes[3] > 0xBF) { // Illegal byte 3
                if (CAN_START_SEQ(bytes[3])) {
                    return 3;
                }
            }
            return 4;
        }
        return 3;
    }
    return 2;
}
