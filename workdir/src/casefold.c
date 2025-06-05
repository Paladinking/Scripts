#include "args.h"
#include "mem.h"
#include "printf.h"
#include "dynamic_string.h"
#include "unicode/unicode_utils.h"




int main() {
    wchar_t* cmd = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(cmd, &argc);

    String buf;
    String arg;
    WString utf16;
    if (!String_create(&buf)) {
        Mem_free(cmd);
        _wprintf_e(L"Out of memory\n");
        ExitProcess(1);
    }
    if (!String_create(&arg)) {
        String_free(&buf);
        Mem_free(cmd);
        _wprintf_e(L"Out of memory\n");
        ExitProcess(1);
    }
    if (!WString_create(&utf16)) {
        String_free(&buf);
        String_free(&arg);
        Mem_free(cmd);
        _wprintf_e(L"Out of memory\n");
        ExitProcess(1);
    }

    for (uint32_t ix = 1; ix < argc; ++ix) {
        if (!String_from_utf16_str(&arg, argv[ix])) {
            continue;
        }
        buf.length = 0;
        for (uint32_t i = 0; i < arg.length;) {
            if (!String_reserve(&buf, buf.length + 4)) {
                continue;
            }
            uint32_t l = get_utf8_seq((uint8_t*)arg.buffer + i, arg.length - i);
            buf.length += unicode_case_fold_utf8((uint8_t*)arg.buffer + i, l,
                                                 (uint8_t*)buf.buffer + buf.length);
            i += l;
        }
        if (WString_from_utf8_bytes(&utf16, buf.buffer, buf.length)) {
            if (WString_append(&utf16, '\n')) {
                outputw(utf16.buffer, utf16.length);
            }
        }
    }

    WString_free(&utf16);
    String_free(&arg);
    String_free(&buf);

    Mem_free(argv);
    ExitProcess(0);
}
