#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "args.h"
#include "glob.h"
#include "printf.h"


int main() {
    const wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = glob_command_line(args, &argc);
    for (uint32_t ix = 0; ix < argc; ++ix) {
        _wprintf(L"%s\n", argv[ix]);
    }

    Mem_free(argv);

    return 0;
}
