#include "printf.h"
#include "mem.h"
#include "args.h"
#include "glob.h"
#include "dbghelp.h"
#include "linkedu64hashmap.h"
#include "u64hashmap.h"
#include "linkedwhashmap.h"

int main();

int do_something() {
    /*HANDLE p = GetCurrentProcess();

    if (!SymInitialize(p, "bin", TRUE)) {

        _printf("Failed init\n");
        return 0;
    }


    SYMBOL_INFO* sym = Mem_alloc(sizeof(SYMBOL_INFO) + 256);
    memset(sym, 0, sizeof(SYMBOL_INFO));
    sym->MaxNameLen = 255;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    if (!SymFromAddr(p, (DWORD64)do_something, 0, sym)) {
        _printf("Failed, %lu\n", GetLastError());
        return 0;
    }

    _printf("Sym: %s\n", sym->Name);


    if (!SymFromAddr(p, (DWORD64)main, 0, sym)) {
        _printf("Failed, %lu\n", GetLastError());
        return 0;
    }

    _printf("Sym: %s\n", sym->Name);
    Mem_free(sym);



    return 1;*/
}




int main() {
    wchar_t* args = GetCommandLineW();
    int argc;

    wchar_t** argv = parse_command_line(args, &argc);
    if (argc < 2) {
        _wprintf(L"Missing argument\n");
        return 1;
    }

    GlobCtx ctx;
    Glob_begin(argv[1], &ctx);
    Path* path;
    bool match = false;
    while (Glob_next(&ctx, &path)) {
         match = true;
        _wprintf(L"%s\n", path->path.buffer);
    }

    if (!match) {
        _wprintf(L"No matches\n");
    }

    return 0;
}
