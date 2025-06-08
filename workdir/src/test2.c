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

    uint64_t target = 123;
    if (argc >= 2) {
        if (!parse_uintw(argv[1], &target, 10)) {
            target = 123;
        }
    }

    U64HashMap a;
    U64HashMap_Create(&a);

    for (uint32_t i = 0; i < 10000; ++i) {
        char* buf = Mem_alloc(16);
        memset(buf, 0, 16);
        if (i < 10) {
            buf[0] = i + '0';
        } else if (i < 100) {
            buf[0] = (i / 10) + '0';
            buf[1] = (i % 10) + '0';
        } else if (i < 1000) {
            buf[0] = (i / 100) + '0';
            buf[1] = ((i / 10) % 10) + '0';
            buf[2] = (i % 10) + '0';
        } else {
            buf[0] = (i / 1000) + '0';
            buf[1] = ((i / 100) % 10) + '0';
            buf[2] = ((i / 10) % 10) + '0';
            buf[3] = (i % 10) + '0';
        }
        U64HashMap_Insert(&a, i, buf);
    }

    _wprintf(L"%llu: '%S'\n", target, U64HashMap_Value(&a, target));

    do_something();

    Mem_free(argv);

    LineCtx ctx1, ctx2;
    ctx1.file = CreateFileW(L"C:\\Users\\axelh\\Scripts\\workdir\\bin\\test_o1.exe", GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (!SyncLineIter_begin(&ctx1, ctx1.file)) {
        _wprintf(L"Failed sync\n");
        return 1;
    }
    if (!LineIter_begin(&ctx2, L"C:\\Users\\axelh\\Scripts\\workdir\\bin\\test_o1.exe")) {
        _wprintf(L"Failed async\n");
        return 1;
    }
    uint64_t l1, l2;
    uint64_t l = 0;
    while (1) {
        ++l;
        char* s1 = SyncLineIter_next(&ctx1, &l1);
        char* s2 = LineIter_next(&ctx2, &l2);

        if (s1 == NULL) {
            if (s2 == NULL) {
                break;
            }
            _wprintf(L"Line %llu: First iterator ran out\n", l);
            break;
        } else if (s2 == NULL) {
            _wprintf(L"Line %llu: Second iterator ran out\n", l);
            break;
        }

        if (l1 != l2) {
            _wprintf(L"Line %llu: Missmatching length %llu != %llu\n", l, l1, l2);
            continue;
        }

        if (memcmp(s1, s2, l1) != 0) {
            _wprintf(L"Line %llu: not equal\n", l);
            continue;
        }
        _wprintf(L"Line %llu same\n", l);
    }

    return 0;
}
