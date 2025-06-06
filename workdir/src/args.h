#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#define FLAG_NO_VALUE 0
#define FLAG_OPTONAL_VALUE 1
#define FLAG_REQUIRED_VALUE 2
#define FLAG_INT 4
#define FLAG_UINT 8
//#define FLAG_DOUBLE 16
#define FLAG_STRING 32
#define FLAG_ENUM 64

#define FLAG_AMBIGUOS 3
#define FLAG_UNKOWN 4
#define FLAG_MISSING_VALUE 5
#define FLAG_INVALID_VALUE 6
#define FLAG_AMBIGUOS_VALUE 7
#define FLAG_UNEXPECTED_VALUE 8

typedef struct EnumValue {
    const wchar_t** values;
    unsigned count;
} EnumValue;

typedef struct FlagValue {
    unsigned type; // Input
    EnumValue * enum_values; // Input
    unsigned enum_count; // Input
    char has_value; // Output
    union { // Output
        wchar_t* str;
        uint64_t uint;
        int64_t sint;
        double real;
    };
} FlagValue;

typedef struct FlagInfo {
    wchar_t short_name; // Input
    const wchar_t* long_name;
    FlagValue* value; // Input / output

    uint32_t shared; // Internal

    uint32_t count; // Output
    uint32_t ord; // Output
} FlagInfo;

typedef struct ErrorInfo {
    uint32_t type;
    uint32_t ix;
    BOOL long_flag;
    wchar_t* value;
} ErrorInfo;


#define BASE_FROM_PREFIX 0xff
/**
 * Parse string containing a signed or unsigned integer.
 * Returns False if the string is not just a valid number.
 * Base can be 16, 10, 8, or BASE_FROM_PREFIX.
 * Any other value is seen as BASE_FROM_PREFIX.
 */
BOOL parse_uintw(const wchar_t* s, uint64_t* i, uint8_t base);
BOOL parse_sintw(const wchar_t* s, int64_t* i, uint8_t base);

/**
 * Find all flags described by <flag_count> entries in <flags>.
 * Return False on error.
 */
BOOL find_flags(wchar_t** argv, int* argc, FlagInfo* flags, uint32_t flag_count, ErrorInfo* err);

/* 
 * Turn error into readable string. Free afterwards.
 */
wchar_t* format_error(ErrorInfo* err, FlagInfo* flags, uint32_t flag_count);

/**
 * Checks if flag <flag> or <long_flag> apears in <argv>.
 * <argc> specifies number of entries in <argv>
 * Return number of occurances. Removes all found instances.
 */
DWORD find_flag(LPWSTR* argv, int* argc, LPCWSTR flag, LPCWSTR long_flag);

/**
 * Get the value of flag <flag> or <long_flag>.
 * <flag> allows specifying value in same agument, e.g -v10 <=> -v 10.
 * Either <flag> or <long_flag> can be NULL.
 * Puts found value in <dest>, will point to value
 * If this function retuns 1, value and flag are removed from args.
 * Returns 0 if no flag found, -1 if flag but no value found, 1 on success.
 */
int find_flag_value(LPWSTR *argv, int *argc, LPCWSTR flag, LPCWSTR long_flag, LPWSTR* dest);


BOOL get_arg_len(const wchar_t* cmd, size_t* ix, size_t* len, BOOL* quoted, unsigned flags);

// Writes to dest, returns one after last written in dest, or NULL if arg could not be parsed
wchar_t* get_arg(const wchar_t* cmd, size_t *ix, wchar_t* dest, unsigned flags);
/**
 * Convert a commandline into C-style argv and argc.
 * Returns argv, argc gets number of arguments.
 */
LPWSTR* parse_command_line(LPCWSTR args, int* argc);

#define ARG_OPTION_BACKSLASH_ESCAPE 1
#define ARG_OPTION_TERMINAL_OPERANDS 2

#define ARG_OPTION_STD (ARG_OPTION_BACKSLASH_ESCAPE)
/**
 * Parse a commandline potentialy without escaping of backslashes.
 * Do not use ARG_OPTION_TERMINAL_OPERANDS, as "&&" and && becomes identical.
 * Use get_arg and get_arg_len directly for that, look at quoted parameter.
 */
LPWSTR* parse_command_line_with(LPCWSTR args, int* argc, unsigned options);
