#include "coff.h"
#include "printf.h"
#include "args.h"
#include "mem.h"

int main() {
    LPWSTR arg = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(arg, &argc);
    if (argc < 2) {
        _wprintf_e(L"Missing argument\n");
        return 1;
    }
    wchar_t *filename = argv[1];

    uint32_t count;
    enum SymbolFileType type;
    char** syms = symbol_dump(filename, &count, &type);
    if (syms == NULL) {
        if (type == SYMBOL_DUMP_NOT_FOUND) {
            _wprintf_e(L"Failed opening file '%s'\n", filename);
        } else if (type == SYMBOL_DUMP_ARCHIVE) {
            _wprintf_e(L"Failed reading archive\n");
        } else if (type == SYMBOL_DUMP_INVALID) {
            _wprintf_e(L"Failed reading file header\n");
        } else if (type == SYMBOL_DUMP_OBJECT) {
            _wprintf_e(L"Failed reading symbol table\n");
        } else if (type == SYMBOL_DUMP_IMAGE) {
            _wprintf_e(L"Failed reading export table\n");
        } else {
            _wprintf_e(L"Unkown error\n");
        }
    } else {
        for (uint32_t ix = 0; ix < count; ++ix) {
            _wprintf(L"%S\n", syms[ix]);
        }
        Mem_free(syms);
    }
    Mem_free(argv);

    return 0;
}
