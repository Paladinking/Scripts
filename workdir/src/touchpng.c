#include <stdint.h>
#include "args.h"
#include "mem.h"
#include "glob.h"
#include "printf.h"

uint8_t empty_png[] = {
    137, 80,  78,  71,  13,  10,  26,  10,  0,   0,   0,   13,  73,  72,  68,
    82,  0,   0,   0,   32,  0,   0,   0,   32,  4,   3,   0,   0,   0,   129,
    84,  103, 199, 0,   0,   0,   1,   115, 82,  71,  66,  0,   174, 206, 28,
    233, 0,   0,   0,   4,   103, 65,  77,  65,  0,   0,   177, 143, 11,  252,
    97,  5,   0,   0,   0,   3,   80,  76,  84,  69,  255, 255, 255, 167, 196,
    27,  200, 0,   0,   0,   9,   112, 72,  89,  115, 0,   0,   14,  195, 0,
    0,   14,  195, 1,   199, 111, 168, 100, 0,   0,   0,   25,  116, 69,  88,
    116, 83,  111, 102, 116, 119, 97,  114, 101, 0,   80,  97,  105, 110, 116,
    46,  78,  69,  84,  32,  53,  46,  49,  46,  49,  50,  19,  1,   71,  116,
    0,   0,   0,   184, 101, 88,  73,  102, 73,  73,  42,  0,   8,   0,   0,
    0,   5,   0,   26,  1,   5,   0,   1,   0,   0,   0,   74,  0,   0,   0,
    27,  1,   5,   0,   1,   0,   0,   0,   82,  0,   0,   0,   40,  1,   3,
    0,   1,   0,   0,   0,   3,   0,   0,   0,   49,  1,   2,   0,   17,  0,
    0,   0,   90,  0,   0,   0,   105, 135, 4,   0,   1,   0,   0,   0,   108,
    0,   0,   0,   0,   0,   0,   0,   163, 147, 0,   0,   232, 3,   0,   0,
    163, 147, 0,   0,   232, 3,   0,   0,   80,  97,  105, 110, 116, 46,  78,
    69,  84,  32,  53,  46,  49,  46,  49,  50,  0,   0,   3,   0,   0,   144,
    7,   0,   4,   0,   0,   0,   48,  50,  51,  48,  1,   160, 3,   0,   1,
    0,   0,   0,   1,   0,   0,   0,   5,   160, 4,   0,   1,   0,   0,   0,
    150, 0,   0,   0,   0,   0,   0,   0,   2,   0,   1,   0,   2,   0,   4,
    0,   0,   0,   82,  57,  56,  0,   2,   0,   7,   0,   4,   0,   0,   0,
    48,  49,  48,  48,  0,   0,   0,   0,   7,   119, 126, 158, 182, 204, 218,
    20,  0,   0,   0,   16,  73,  68,  65,  84,  40,  207, 99,  24,  5,   163,
    0,   39,  96,  96,  0,   0,   2,   32,  0,   1,   8,   227, 79,  222, 0,
    0,   0,   0,   73,  69,  78,  68,  174, 66,  96,  130};

void file_error(DWORD e, const ochar_t* arg) {
    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_BAD_ARGUMENTS ||
        e == ERROR_PATH_NOT_FOUND) {
        oprintf_e("Cannot create '%s': No such file or directory\n", arg);
    } else if (e == ERROR_ACCESS_DENIED) {
        oprintf_e("Cannot create '%s': Permission denied\n", arg);
    } else {
        wchar_t buf[256];
        if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, e, 0, buf, 256, NULL)) {
            oprintf_e("Cannot create '%s': " WFMT, arg, buf);
        } else {
            oprintf_e("Cannot create '%s'\n", arg);
        }
    }
}

const ochar_t *HELP_MESSAGE =
    oL("Usage: %s [OPTION]... [FILE]...\n")
    oL("Create PNG image file(s) FILE(s)\n\n")
    oL("Creates an empty 32x32 png file for each FILE, if FILE does not already exist\n\n")
    oL("-f, --force              make an empty png even if file already exists\n")
    oL("-h, --help               show this message and exit\n");

int touchpng(ochar_t** argv, int argc) {
    FlagInfo flags[] = {
        {oL('h'), oL("help"), NULL},
        {oL('f'), oL("force"), NULL}
    };
    uint32_t flag_count = sizeof(flags) / sizeof(FlagInfo);
    ErrorInfo err;
    if (!find_flags(argv, &argc, flags, flag_count, &err)) {
        ochar_t* e = format_error(&err, flags, flag_count);
        if (e != NULL) {
            oprintf_e("%s\n", e);
            oprintf_e("Run '%s --help' for more information\n", argv[0]);
        }
        Mem_free(e);
        return ERROR_BAD_ARGUMENTS;
    }
    if (flags[0].count > 0) {
        oprintf_ostr(HELP_MESSAGE, argv[0]);
        return 0;
    }

    int status = 0;

    for (int ix = 1; ix < argc; ++ix) {
        ochar_t* filename = argv[ix];
        if (!is_file(filename) || flags[1].count > 0) {
            if (!write_file(filename, empty_png, sizeof(empty_png))) {
                status = GetLastError();
                file_error(status, filename);
            }
        }
    }

    return status;
}

entrypoint(touchpng);
