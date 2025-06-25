#define UNICODE
#include "args.h"
#include "mem.h"

#ifndef NARROW_OCHAR
#define ostrcmp wcscmp
#define ostrlen wcslen
#define ostrchr wcschr
#define ostrncmp wcsncmp
#else
#define ostrcmp strcmp
#define ostrlen strlen
#define ostrchr strchr
#define ostrncmp strncmp
#endif

bool parse_uint(const ochar_t* s, uint64_t* i, uint8_t base) {
    *i = 0;
    uint64_t n = 0;
    if (*s == oL('\0')) {
        return false;
    }
    if (base != 10 && base != 8 && base != 16) {
        if (*s == oL('0') && (*(s + 1) == oL('x') || *(s + 1) == oL('X'))) {
            base = 16;
            s += 2;
        } else if (*s == oL('0')) {
            base = 8;
            s += 1;
        } else {
            base = 10;
        }
    }

    if (base == 16) { // Hex
        while (*s != oL('\0')) {
            ochar_t c = *s;
            if (n > 0xfffffffffffffff) {
                return false; // Overflow
            }
            n = n << 4;
            if (c >= oL('0') && c <= oL('9')) {
                n += c - oL('0');
            } else if (c >= oL('a') && c <= oL('f')) {
                n += (c - oL('a')) + 10;
            } else if (c >= oL('A') && c <= oL('F')) {
                n += (c - oL('A')) + 10;
            } else {
                return false;
            }
            ++s;
        }
    } else if (base == 8) { // Octal
        while (*s != oL('\0')) {
            ochar_t c = *s;
            if (n > 0x1fffffffffffffff) {
                return false; // Overflow
            }
            n = n << 3;
            if (c >= oL('0') && c <= oL('7')) {
                n += c - oL('0');
            } else {
                return false;
            }
            ++s;
        }
    } else { // Base 10
        while (*s != oL('\0')) {
            ochar_t c = *s;
            if (c < oL('0') || c > oL('9')) {
                return false;
            }
            uint64_t v = c - oL('0');
            if (n > 0x1999999999999999) {
                return false;
            } else if (n == 0x1999999999999999) {
                if (v > 5) {
                    return false;
                }
            }
            n = n * 10;
            n += v;
            ++s;
        }
    }
    *i = n;
    return true;
}

bool parse_sint(const ochar_t* s, int64_t* i, uint8_t base) {
    bool negative = false;
    if (*s == oL('-')) {
        negative = true;
        ++s;
    }
    uint64_t u;
    if (!parse_uint(s, &u, base)) {
        return false;
    }
    if (negative) {
        if (u > 0x8000000000000000) {
            return false;
        } else if (u == 0x8000000000000000) {
            *i = INT64_MIN;
        } else {
            *i = -((int64_t)u);
        }
    } else {
        if (u > INT64_MAX) {
            return false;
        } else {
            *i = (int64_t) u;
        }
    }
    return true;
}


// Counts the number of leading matching characters.
unsigned ostrmatch(const ochar_t* a, const ochar_t* b) {
    unsigned ix = 0;
    while (a[ix] != oL('\0') && b[ix] != oL('\0')) {
        if (a[ix] != b[ix]) {
            return ix;
        }
        ++ix;
    }
    return ix;
}

uint32_t prefix_len(const ochar_t* str, const ochar_t* full, bool has_val) {
    unsigned len;
    if (has_val) {
        const ochar_t* eq = ostrchr(str, '=');
        if (eq == NULL) {
            len = ostrlen(str);
        } else {
            len = eq - str;
        }
    } else {
        len = ostrlen(str);
    }
    if (ostrncmp(str, full, len) == 0) {
        return len;
    }
    return 0;
}

bool parse_argument(ochar_t* val, FlagValue* valid, unsigned ix, ErrorInfo* err, ochar_t** slot,
                    FlagInfo* flags, uint32_t flag_count) {
    if (valid->type & FLAG_STRING) {
        valid->str = val;
        valid->has_value = 1;
        valid->count = 1;
        return true;
    }
    if (valid->type & FLAG_STRING_MANY) {
        valid->has_value  = 1;
        if (valid->count == 0) {
            valid->count = 1;
            slot[0] = val;
            valid->strlist = slot;
            return true;
        }
        ochar_t** s = slot + 1;
        while (1) {
            int flag_add = 1;
            for (uint32_t i = 0; i < flag_count; ++i) {
                if (flags[i].value == NULL || !(flags[i].value->type & FLAG_STRING_MANY) ||
                    flags[i].value->count == 0) {
                    continue;
                }
                if (flags[i].value->strlist == s) {
                    for (uint32_t j = 0; j < flags[i].value->count; ++j) {
                        s[-1] = s[0];
                        ++s;
                    }
                    flags[i].value->strlist = flags[i].value->strlist - 1;
                    if (i == ix) {
                        s[-1] = val;
                        valid->count += 1;
                        return true;
                    }
                    flag_add = 0;
                    break;
                }
            }
            s = s + flag_add;
        }
    }

    if (valid->type & FLAG_INT) {
        if (!parse_sint(val, &valid->sint, BASE_FROM_PREFIX)) {
            err->type = FLAG_INVALID_VALUE;
            err->ix = ix;
            err->value = val;
            return false;
        }
        valid->has_value = 1;
        valid->count = 1;
        return true;
    }
    if (valid->type & FLAG_UINT) {
        if (!parse_uint(val, &valid->uint, BASE_FROM_PREFIX)) {
            err->type = FLAG_INVALID_VALUE;
            err->ix = ix;
            err->value = val;
            return false;
        }
        valid->has_value = 1;
        valid->count = 1;
        return true;
    }
    if (!(valid->type & FLAG_ENUM)) {
        err->type = FLAG_INVALID_VALUE;
        err->ix = ix;
        err->value = val;
        return false;
    }

    EnumValue* vals = valid->enum_values;
    unsigned count = valid->enum_count;

    unsigned longest_prefix = 0;
    unsigned longest_ix = 0;
    bool collision = false;
    for (unsigned j = 0; j < count; ++j) {
        for (unsigned i = 0; i < vals[j].count; ++i) {
            uint32_t len = prefix_len(val, vals[j].values[i], false);
            if (len == 0) {
                continue;
            }
            if (len > longest_prefix) {
                longest_prefix = len;
                longest_ix = j;
                collision = false;
            } else if (len == longest_prefix) {
                if (longest_ix != j) {
                    collision = true;
                }
            }
        }
    }
    if (longest_prefix == 0) {
        err->type = FLAG_INVALID_VALUE;
        err->value = val;
        err->ix = ix;
        return false;
    } 
    if (collision) {
        err->type = FLAG_AMBIGUOS_VALUE;
        err->value = val;
        err->ix = ix;
        return false;
    }
    valid->has_value = 1;
    valid->uint = longest_ix;
    valid->count = 1;
    return true;
}

bool find_flags(ochar_t** argv, int* argc, FlagInfo* flags, uint32_t flag_count, ErrorInfo* err) {
    if (argv == NULL) {
        err->type = FLAG_NULL_ARGV;
        err->ix = 0;
        err->long_flag = false;
        err->value = NULL;
        return false;
    }
    unsigned order = 1;
    err->long_flag = false;
    for (unsigned ix = 0; ix < flag_count; ++ix) {
        flags[ix].count = 0;
        flags[ix].ord = 0;
        flags[ix].shared = 0;
        if (flags[ix].value != NULL) {
            flags[ix].value->has_value = 0;
        }
    }
    for (unsigned ix = 0; ix < flag_count; ++ix) {
        if (flags[ix].long_name == NULL) {
            continue;
        }
        for (unsigned j = ix + 1; j < flag_count; ++j) {
            if (flags[j].long_name == NULL) {
                continue;
            }
            unsigned match = ostrmatch(flags[ix].long_name, flags[j].long_name);
            if (match > flags[ix].shared) {
                flags[ix].shared = match;
            }
            if (match > flags[j].shared) {
                flags[j].shared = match;
            }
        }
    }

    for (int ix = 0; ix < *argc; ++ix) {
        if (argv[ix][0] != oL('-') || argv[ix][1] == oL('\0')) {
            continue;
        }
        ochar_t* arg = argv[ix];
        for (int j = ix + 1; j < *argc; ++j) {
            argv[j - 1] = argv[j];
        }
        --ix;
        --(*argc);
        if (arg[1] == oL('-')) {
            if (arg[2] == oL('\0')) {
                return true;
            }
            unsigned char found = 0;
            for (unsigned j = 0; j < flag_count; ++j) {
                if (flags[j].long_name == NULL) {
                    continue;
                }
                uint32_t len = prefix_len(arg + 2, flags[j].long_name, true);
                if (len == 0) {
                    continue;
                }
                if (len <= flags[j].shared &&
                    len != ostrlen(flags[j].long_name)) {
                    err->type = FLAG_AMBIGUOS;
                    err->value = arg;
                    if (arg[len + 2] == oL('=')) {
                        arg[len + 2] = oL('\0');
                    }
                    err->ix = j;
                    err->long_flag = true;
                    return false;
                }
                if (flags[j].value != NULL) {
                    if (arg[len + 2] == oL('\0')) {
                        ++flags[j].count;
                        flags[j].ord = order++;
                        found = 1;
                        if (flags[j].value->type & FLAG_OPTONAL_VALUE) {
                            flags[j].value->has_value = 0;
                            break;
                        }
                        // arg is in pos ix + 1
                        if (ix + 1 < *argc) {
                            ochar_t* val = argv[ix + 1];
                            for (int j = ix + 1; j + 1 < *argc; ++j) {
                                argv[j] = argv[j + 1];
                            }
                            --(*argc);
                            if (!parse_argument(val, flags[j].value, j, err, argv + *argc,
                                                flags, flag_count)) {
                                err->long_flag = true;
                                return false;
                            }
                        } else {
                            err->type = FLAG_MISSING_VALUE;
                            err->value = arg;
                            err->ix = j;
                            err->long_flag = true;
                            return false;
                        }
                    } else if (arg[len + 2] == oL('=')) {
                        ++flags[j].count;
                        flags[j].ord = order++;
                        found = 1;
                        if (!parse_argument(arg + len + 3, flags[j].value, j, err, argv + *argc,
                                            flags, flag_count)) {
                            err->long_flag = true;
                            return false;
                        }
                    } else {
                        continue;
                    }
                    break;
                } else {
                    if (arg[len + 2] == oL('=')) {
                        err->type = FLAG_UNEXPECTED_VALUE;
                        err->ix = j;
                        err->value = arg;
                        err->long_flag = true;
                        arg[len + 2] = oL('\0');
                        return false;
                    }
                    ++flags[j].count;
                    flags[j].ord = order++;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                err->type = FLAG_UNKOWN;
                err->value = arg;
                err->ix = flag_count;
                err->long_flag = true;
                return false;
            }
        } else {
            for (int i = 1; arg[i] != oL('\0'); ++i) {
                unsigned char found = 0;
                unsigned j = 0;
                for (; j < flag_count; ++j) {
                    if (flags[j].short_name == arg[i]) {
                        ++flags[j].count;
                        flags[j].ord = order++;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    err->type = FLAG_UNKOWN;
                    err->value = arg;
                    err->ix = flag_count;
                    arg[0] = arg[i];
                    arg[1] = oL('\0');
                    return false;
                } else if (flags[j].value != NULL) {
                    if (arg[i + 1] == oL('\0')) {
                        if (ix + 1 == *argc)  {
                            if (!(flags[j].value->type & FLAG_OPTONAL_VALUE)) {
                                err->type = FLAG_MISSING_VALUE;
                                err->value = arg;
                                err->ix = j;
                                arg[0] = arg[i];
                                arg[1] = oL('\0');
                                return false;
                            }
                            flags[j].value->has_value = 0;
                        } else {
                            ochar_t* val = argv[ix + 1];
                            for (int j = ix + 1; j + 1 < *argc; ++j) {
                                argv[j] = argv[j + 1];
                            }
                            --(*argc);
                            if (!parse_argument(val, flags[j].value, j, err, argv + *argc,
                                                flags, flag_count)) {
                                return false;
                            }
                            break;
                        }
                    } else {
                        if (!parse_argument(arg + i + 1, flags[j].value, j, err, argv + *argc,
                                            flags, flag_count)) {
                            return false;
                        }
                        break;
                    }
                }
            }
        }
    }
    return true;
}

ochar_t* format_error(ErrorInfo* err, FlagInfo* flags, uint32_t flag_count) {
    ochar_t* str;
    if (err->type == FLAG_NULL_ARGV) {
        str = Mem_alloc(23 * sizeof(ochar_t));
        if (str == NULL) {
            return NULL;
        }
        memcpy(str, oL("argument list was NULL"), 22 * sizeof(ochar_t));
        str[22] = oL('\0');
        return str;
    }

    uint32_t val_len = ostrlen(err->value);
    if (err->type == FLAG_AMBIGUOS) {
        uint32_t len = 8 + val_len + 31;
        for (uint32_t ix = 0; ix < flag_count; ++ix) {
            if (flags[ix].long_name == NULL) {
                continue;
            }
            if (ostrmatch(err->value + 2, flags[ix].long_name) == val_len - 2) {
                len += ostrlen(flags[ix].long_name) + 5;
            }
        }
        str = Mem_alloc(len * sizeof(ochar_t));
        if (str == NULL) {
            return NULL;
        }
        memcpy(str, oL("option '"), 8 * sizeof(ochar_t));
        memcpy(str + 8, err->value, val_len * sizeof(ochar_t));
        memcpy(str + 8 + val_len, oL("' is ambiguous; possibilities: "),
               31 * sizeof(ochar_t));
        len = 31 + 8 + val_len;
        for (uint32_t i = 0; i < flag_count; ++i) {
            if (flags[i].long_name == NULL) {
                continue;
            }
            if (ostrmatch(err->value + 2, flags[i].long_name) == val_len - 2) {
                uint32_t l = ostrlen(flags[i].long_name);
                memcpy(str + len, oL("'--"), 3 * sizeof(ochar_t));
                memcpy(str + len + 3, flags[i].long_name, l * sizeof(ochar_t));
                str[len + 3 + l] = oL('\'');
                str[len + 4 + l] = oL(' ');
                len += l + 5;
            }
        }
        str[len - 1] = '\0';
        return str;
    } else if (err->type == FLAG_UNKOWN) {
        uint32_t len = 15 + val_len + 2;
        str = Mem_alloc(len * sizeof(ochar_t));
        if (str == NULL) {
            return NULL;
        }
        memcpy(str, oL("unkown option '"), 15 * sizeof(ochar_t));
        memcpy(str + 15, err->value, val_len * sizeof(ochar_t));
        memcpy(str + 15 + val_len, oL("'"), 2 * sizeof(ochar_t));
        return str;
    } 
    uint32_t flag_len;
    if (err->long_flag) {
        flag_len = ostrlen(flags[err->ix].long_name) + 2;
    } else {
        flag_len = 1;
    }

    if (err->type == FLAG_MISSING_VALUE) {
        uint32_t len = 8 + flag_len + 23;
        str = Mem_alloc(len * sizeof(ochar_t));
        if (str == NULL) {
            return NULL;
        }
        memcpy(str, oL("option '"), 8 * sizeof(ochar_t));
        if (err->long_flag) {
            str[8] = oL('-');
            str[9] = oL('-');
            memcpy(str + 10, flags[err->ix].long_name, 
                   (flag_len - 2) * sizeof(ochar_t));
        } else {
            str[8] = flags[err->ix].short_name;
        }
        memcpy(str + 8 + flag_len, oL("' requires an argument"),
               23 * sizeof(ochar_t));
        return str; 
    } else if (err->type == FLAG_INVALID_VALUE) {
        uint32_t len = 18 + 7 + 2 + flag_len + val_len;
        str = Mem_alloc(len * sizeof(ochar_t));
        if (str == NULL) {
            return NULL;
        }
        memcpy(str, oL("invalid argument '"), 18 * sizeof(ochar_t));
        memcpy(str + 18, err->value, val_len * sizeof(ochar_t));
        memcpy(str + 18 + val_len, oL("' for '"), 7 * sizeof(ochar_t));
        if (err->long_flag) {
            str[25 + val_len] = oL('-');
            str[26 + val_len] = oL('-');
            memcpy(str + 27 + val_len, flags[err->ix].long_name, 
                   (flag_len - 2) * sizeof(ochar_t));
        } else {
            str[25 + val_len] = flags[err->ix].short_name;
        }
        memcpy(str + 25 + val_len + flag_len, oL("'"), 2 * sizeof(ochar_t));
        return str;
    } else if (err->type == FLAG_AMBIGUOS_VALUE) {
        uint32_t len = 20 + 7 + 2 + flag_len + val_len;
        str = Mem_alloc(len * sizeof(ochar_t));
        if (str == NULL) {
            return NULL;
        }
        memcpy(str, oL("ambiguous argument '"), 20 * sizeof(ochar_t));
        memcpy(str + 20, err->value, val_len * sizeof(ochar_t));
        memcpy(str + 20 + val_len, oL("' for '"), 7 * sizeof(ochar_t));
        if (err->long_flag) {
            str[27 + val_len] = oL('-');
            str[28 + val_len] = oL('-');
            memcpy(str + 29 + val_len, flags[err->ix].long_name, 
                   (flag_len - 2) * sizeof(ochar_t));
        } else {
            str[27 + val_len] = flags[err->ix].short_name;
        }
        memcpy(str + 27 + val_len + flag_len, oL("'"), 2 * sizeof(ochar_t));
        return str;
    } else if (err->type == FLAG_UNEXPECTED_VALUE) {
        uint32_t len = 25 + flag_len + 2;
        str = Mem_alloc(len * sizeof(ochar_t));
        if (str == NULL) {
            return NULL;
        }
        memcpy(str, oL("unexpected argument for '"), 25 * sizeof(ochar_t));
        if (err->long_flag) {
            str[25] = oL('-');
            str[26] = oL('-');
            memcpy(str + 27, flags[err->ix].long_name, 
                   (flag_len - 2) * sizeof(ochar_t));
        } else {
            str[25] = flags[err->ix].short_name;
        }
        memcpy(str + 25 + flag_len, oL("'"), 2 * sizeof(ochar_t));
        return str;
    }
    
    return NULL;
}

int find_flag(ochar_t** argv, int *argc, const ochar_t* flag, const ochar_t* long_flag) {
    int count = 0;
    for (int i = 1; i < *argc; ++i) {
        if (ostrcmp(argv[i], flag) == 0 || ostrcmp(argv[i], long_flag) == 0) {
            count += 1;
            for (int j = i + 1; j < *argc; ++j) {
                argv[j - 1] = argv[j];
            }
            --(*argc);
            --i;
        }
    }
    return count;
}

int find_flag_value(ochar_t** argv, int *argc, const ochar_t* flag, const ochar_t* long_flag,
                    ochar_t** dest) {
    unsigned short_len = ostrlen(flag);
    unsigned long_len = ostrlen(long_flag);
    for (int i = 0; i < *argc; ++i) {
        if (ostrncmp(argv[i], flag, short_len) == 0) {
            if (argv[i][short_len] == oL('\0')) {
                if (i + 1 == *argc) {
                    return -1;
                }
                *dest = argv[i + 1];
                for (int j = i + 2; j < *argc; ++j) {
                    argv[j - 2] = argv[j];
                }
                *argc -= 2;
                return 1;
            } else {
                *dest = argv[i] + short_len;
                for (int j = i + 1; j < *argc; ++j) {
                    argv[j - i] = argv[j];
                }
                *argc -= 1;
                return 1;
            }
        } else if (ostrncmp(argv[i], long_flag, long_len) == 0) {
            if (argv[i][long_len] != oL('\0') && argv[i][long_len] != oL('=')) {
                continue;
            }
            if (argv[i][long_len] == oL('=')) {
                *dest = argv[i] + long_len + 1;
                for (int j = i + 1; j < *argc; ++j) {
                    argv[j - i] = argv[j];
                }
                *argc -= 1;
                return 1;
            } else {
                if (i + 1 == *argc) {
                    return -1;
                }
                *dest = argv[i + 1];
                for (int j = i + 2; j < *argc; ++j) {
                    argv[j - 2] = argv[j];
                }
                *argc -= 2;
                return 1;
            }
        }
    }
    return 0;
}

bool get_arg_len(const ochar_t* cmd, size_t* ix, size_t* len, bool* quoted, unsigned flags) {
    bool terminal_chars = flags & ARG_OPTION_TERMINAL_OPERANDS;
    bool escape_backslash = flags & ARG_OPTION_BACKSLASH_ESCAPE;
    bool in_quotes = false;
    *quoted = false;
    *len = 0;

    while (cmd[*ix] == oL(' ') || cmd[*ix] == oL('\t')) {
        ++(*ix);
    }

    unsigned start = *ix;
    if (cmd[*ix] == oL('\0')) {
        return false;
    }

    if (terminal_chars && (cmd[*ix] == oL('>') || cmd[*ix] == oL('<') || cmd[*ix] == oL('&') || cmd[*ix] == oL('|'))) {
        if (cmd[*ix + 1] == cmd[*ix]) {
            ++(*ix);
            ++(*len);
        }
        ++(*ix);
        ++(*len);
        return true;
    }

    while (1) {
        if (cmd[*ix] == oL('\0')) {
            return true;
        }
        if (!in_quotes && (cmd[*ix] == oL(' ') || cmd[*ix] == oL('\t'))) {
            return true;
        }
        if (!in_quotes && terminal_chars && (cmd[*ix] == oL('>') || 
            cmd[*ix] == oL('<') || cmd[*ix] == oL('&') || cmd[*ix] == oL('|'))) {
            return true;
        }
        if (escape_backslash && cmd[*ix] == oL('\\')) {
            size_t count = 1;
            while (cmd[*ix + count] == oL('\\')) {
                count += 1;
            }
            if (cmd[*ix + count] == oL('"')) {
                if (count % 2 == 0) {
                    *len += count / 2;
                } else {
                    *len += count / 2 + 1;
                    ++(*ix);
                }
            } else {
                *len += count;
            }
            *ix += count;
        } else if (cmd[*ix] == oL('"')) {
            *quoted = true;
            if (in_quotes && cmd[*ix + 1] == oL('"')) {
                ++(*len);
                ++(*ix);
                in_quotes = !in_quotes;
            } else {
                in_quotes = !in_quotes;
            }
            ++(*ix);

        } else {
            ++(*ix);
            ++(*len);
        }
    }
}

ochar_t* get_arg(const ochar_t* cmd, size_t *ix, ochar_t* dest, unsigned flags) {
    bool in_quotes = false;
    bool terminal_chars = flags & ARG_OPTION_TERMINAL_OPERANDS;
    bool escape_backslash = flags & ARG_OPTION_BACKSLASH_ESCAPE;

    while (cmd[*ix] == oL(' ') || cmd[*ix] == oL('\t')) {
        ++(*ix);
    }

    unsigned start = *ix;
    if (cmd[*ix] == oL('\0')) {
        return NULL;
    }

    if (terminal_chars && (cmd[*ix] == oL('>') || cmd[*ix] == oL('<') || cmd[*ix] == oL('&') || cmd[*ix] == oL('|'))) {
        if (cmd[*ix + 1] == cmd[*ix]) {
            ++(*ix);
            dest[0] = cmd[*ix];
            ++dest;
        }
        dest[0] = cmd[*ix];
        dest[1] = oL('\0');
        ++(*ix);
        return dest + 2;
    }

    while (1) {
        if (cmd[*ix] == oL('\0')) {
            dest[0] = oL('\0');
            ++dest;
            return dest;
        }
        if (!in_quotes && (cmd[*ix] == oL(' ') || cmd[*ix] == oL('\t'))) {
            dest[0] = oL('\0');
            ++dest;
            return dest;
        }
        if (!in_quotes && terminal_chars && (cmd[*ix] == oL('>') || 
            cmd[*ix] == oL('<') || cmd[*ix] == oL('&') || cmd[*ix] == oL('|'))) {
            dest[0] = oL('\0');
            ++dest;
            return dest;
        }
        if (escape_backslash && cmd[*ix] == oL('\\')) {
            size_t count = 1;
            while (cmd[*ix + count] == oL('\\')) {
                count += 1;
            }
            if (cmd[*ix + count] == oL('"')) {
                for (int i = 0; i < count / 2; ++i) {
                    dest[0] = '\\';
                    ++dest;
                }
                if (count % 2 == 1) {
                    dest[0] = oL('"');
                    ++dest;
                    ++(*ix);
                }
            } else {
                for (int i = 0; i < count; ++i) {
                    dest[0] = oL('\\');
                    ++dest;
                }
            }
            *ix += count;
        } else if (cmd[*ix] == oL('"')) {
            if (in_quotes && cmd[*ix + 1] == oL('"')) {
                dest[0] = oL('"');
                ++dest;
                ++(*ix);
                in_quotes = !in_quotes;
            } else {
                in_quotes = !in_quotes;
            }
            ++(*ix);

        } else {
            dest[0] = cmd[*ix];
            ++(*ix);
            ++dest;
        }
    }
}

ochar_t** parse_command_line_with(const ochar_t* args, int* argc, unsigned options) {
    *argc = 0;
    size_t i = 0;
    size_t size = 0;

    size_t len = 0;
    bool quoted;
    while (get_arg_len(args, &i, &len, &quoted, options)) {
        size += len;
        *argc += 1;
    }

    size = size * sizeof(ochar_t) + (sizeof(ochar_t*) + sizeof(ochar_t)) * (*argc);
    ochar_t** argv = Mem_alloc(size);
    if (argv == NULL) {
        return NULL;
    } 
    ochar_t* args_dest = (ochar_t*) (argv + *argc);
    ochar_t* dest = args_dest;
    i = 0;
    int arg = 0;
    while (1) {
        ochar_t* next = get_arg(args, &i, dest, options);
        if (next == NULL) {
            break;
        }
        argv[arg] = dest;
        ++arg;
        dest = next;
    }
    return argv;
}

ochar_t **parse_command_line(const ochar_t* args, int *argc) {
    return parse_command_line_with(args, argc, ARG_OPTION_STD);
}
