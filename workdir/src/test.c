#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "args.h"
#include "glob.h"
#include "printf.h"


int main() {
    GlobCtx ctx;
    Path* path;
    const wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(args, &argc);

    for (uint32_t ix = 1; ix < argc; ++ix) {
        _wprintf(L"Glob %s:\n", argv[ix]);
        Glob_begin(argv[ix], &ctx);
        while (Glob_next(&ctx, &path)) {
            _wprintf(L"%s\n", path->path.buffer);
        }
    }

    return 0;
}
