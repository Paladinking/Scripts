#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <float.h>
#include <string.h>
#include <math.h>


struct Pow5 {
    int16_t scale;
    int16_t power;
    uint64_t value;
};
const static struct Pow5 pow5[] = {
    {0, 1, 5}, // 5^1
    {0, 2, 25}, // 5^2
    {0, 4, 625}, // 5^4
    {0, 8, 390625}, // 5^8
    {0, 16, 152587890625}, // 5^16
    {-11, 32, 11368683772161602973u}, // 5^32 >> 11
    {-85, 64, 14012984643248170709u}, // 5^64 >> 85
    {-234, 128, 10644899600020376799u}, // 5^128 >> 234
};

const static struct Pow5 invPow5[] = {
    {66,  -1, 14757395258967641293u}, // 5^-1 << 66
    {68,  -2, 11805916207174113035u}, // 5^-2 << 68
    {73,  -4, 15111572745182864684u}, // 5^-4 << 73
    {82,  -8, 12379400392853802749u}, // 5^-8 << 82
    {101, -16, 16615349947311448412u}, // 5^-16 << 101
    {138, -32, 14965776766268445883u}, // 5^-32 << 138
    {212, -64, 12141680576410806694u}, // 5^-64 << 212
    {361, -128, 15983352577617880225u}, // 5^-128 << 361
};

#define WBUF_SIZE 8

int multiply_by(uint32_t* mantissa, int length, uint16_t shift, uint64_t val, int32_t* exp) {
    uint32_t res[WBUF_SIZE * 2] = {0};
    uint64_t low = (uint32_t)val;
    uint64_t high = val >> 32;

    for (int ix = 0; ix < length; ++ix) {
        // multiply in two parts to get full result
        uint64_t lowp = low * mantissa[ix];
        uint64_t highp = high * mantissa[ix];

        int i = ix;
        do {
            lowp += res[i];
            res[i] = lowp;
            lowp = lowp >> 32;
            ++i;
        } while (lowp > 0);
        i = ix + 1;
        do {
            highp += res[i];
            res[i] = highp;
            highp = highp >> 32;
            ++i;
        } while (highp > 0);
    }
    int l = sizeof(res) / sizeof(res[0]);
    while (l > 0 && res[l - 1] == 0) {
        l -= 1;
    }

    *exp = -shift;
    if (l > WBUF_SIZE) {
        memcpy(mantissa, res + (l - WBUF_SIZE), WBUF_SIZE * sizeof(uint32_t));
        *exp += (l - WBUF_SIZE) * 32;
        l = WBUF_SIZE;
    } else {
        memcpy(mantissa, res, l * sizeof(uint32_t));
        memset(mantissa + l, 0, (WBUF_SIZE - l) * sizeof(uint32_t));
    }

    return l;
}


uint8_t round_to_pow2(uint32_t v) {
    if (v >= 128) {
        return 7;
    }
    return (31 - __builtin_clz(v));
}

int scale_by(uint32_t* mantissa, int length, int32_t decimals, int32_t* exp) {
    int e;
    // Offset exponent by decimals to get correct value after multiplying 
    // mantissa by powers of 5 instead of powers of 10.
    *exp = -decimals;
    while (decimals < 0) {
        int ix = round_to_pow2(-decimals);
        decimals += pow5[ix].power;
        length = multiply_by(mantissa, length, pow5[ix].scale, pow5[ix].value, &e);
        *exp += e;
    }
    while (decimals > 0) {
        int ix = round_to_pow2(decimals);
        decimals += invPow5[ix].power;
        length = multiply_by(mantissa, length, invPow5[ix].scale, invPow5[ix].value, &e);
        *exp += e;
    }
    return length;
}

bool parse_float(const char* str, float* value) {
    int length = 0;
    int32_t digits = 0;
    int32_t decimals = -1;
    uint32_t mantissa[WBUF_SIZE] = {0};
    bool negative = false;

    if (*str == '-' || *str == '+') {
        negative = *str == '-';
        ++str;
    } 
    while (true) {
        char c = *(str++);
        if (c >= '0' && c <= '9') {
            uint32_t carry = c - '0';
            int ix = 0;
            do {
                if (ix >= WBUF_SIZE) {
                    // Overflow
                    return false;
                }
                uint64_t res = ((uint64_t)mantissa[ix]) * 10 + carry;
                mantissa[ix] = res;
                carry = (res >> 32);
                ++ix;
            } while (ix < length || carry > 0);
            length = ix;
            if (decimals >= 0) {
                ++decimals;
            } else {
                ++digits;
            }
        } else if (c == '.') {
            if (decimals >= 0) {
                --str;
                break;
            }
            decimals = 0;
        } else {
            --str;
            break;
        }
    }
    if (digits == 0 || decimals == 0) {
        return false;
    }
    if (decimals < 0) {
        decimals = 0;
    }

    int64_t pow10 = 0;
    if (*str == 'e' || *str == 'E') {
        ++str;
        bool negative_exp = false;
        if (*str == '-' || *str == '+') {
            negative_exp = *str == '-';
            ++str;
        }
        if (*str < '0' || *str > '9') {
            // Digit is required
            return false;
        }
        while (*str >= '0' && *str <= '9') {
            uint32_t v = *(str++) - '0';
            if (pow10 < (INT64_MAX - 6) / 10) {
                pow10 = 10 * pow10 + v;
            }
        }
        if (negative_exp) {
            pow10 = -pow10;
        }
    }
    if (length <= 1 && mantissa[0] == 0) {
        *value = negative ? -0.0 : 0.0;
        return true;
    }
    // 10^39 > FLOAT32_MAX, 
    if (pow10 >= INT32_MAX || pow10 - decimals > 40) {
        *value = negative ? -INFINITY : INFINITY;
        return true;
    }
    // Smallest float is ~ 10^-45, and 32-byte buffer can store value up to 10^77
    // Powers smaller than -46 - 76 become 0 always
    if (pow10 <= INT32_MIN || pow10 - decimals < -125) {
        *value = negative ? -0.0 : 0.0;
        return true;
    }
    decimals -= pow10;

    int32_t exp = 0;
    if (decimals != 0) {
        length = scale_by(mantissa, length, decimals, &exp);
    }

    float res = 0.0;
    for (int ix = 0; ix < length; ++ix) {
        // Since uint32_t might not fit into a float, 
        // we need to divide each part into uint16_t pieces
        uint16_t low = mantissa[ix];
        uint16_t high = mantissa[ix] >> 16;
        if (low != 0) {
            res += __builtin_ldexpf(low, ix * 32 + exp);
        }
        if (high != 0) {
            res += __builtin_ldexpf(high, ix * 32 + exp + 16);
        }
    }
    *value = negative ? -res : res;
    return true;
}

int main() {
    float res;
    if (parse_float("340282346638528859811704183484516925440", &res)) {
        printf("%.110f\n", res);
        printf("%.110f\n\n", 340282346638528859811704183484516925440.0f);
    }
    if (parse_float("0.123046875", &res)) {
        printf("%.110f\n", res);
        printf("%.110f\n\n", 0.123046875f);
    }
    if (parse_float("0.000000000000000000867361737988403547205962240695953369140625", &res)) {
        printf("%.110f\n", res);
        printf("%.110f\n\n", 0.000000000000000000867361737988403547205962240695953369140625f);
    }

    if (parse_float("12345.12345", &res)) {
        printf("%.110f\n", res);
        printf("%.110f\n\n", 12345.12345f);
    }

    if (parse_float("999.9999", &res)) {
        printf("%.110f\n", res);
        printf("%.110f\n\n", 999.9999f);
    }
    if (parse_float("999.9999e32", &res)) {
        printf("%.110f\n", res);
        printf("%.110f\n\n", 999.9999e32f);
    }
    if (parse_float("1.40129846e-45", &res)) {
        printf("%.210f\n", res);
        printf("%.210f\n\n", 1.40129846e-45f);
    }

    return 0;
}
