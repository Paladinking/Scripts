#include "subprocess.h"
#include "mem.h"
#include "printf.h"
#include "glob.h"
#include "args.h"


int main() {
    wchar_t* arg = GetCommandLineW();

    uint64_t start = 0;
    size_t len = wcslen(arg);
    size_t exe_len = 0;
    bool quoted = false;

    if (!get_arg_len(arg, &start, &exe_len, &quoted, ARG_OPTION_STD)) {
        _wprintf_e(L"Failed parsing arguments\n");
        ExitProcess(1);
    }
    arg += start;
    len -= start;

    wchar_t filebuf[1024];

    if (!find_file_relative(filebuf, sizeof(filebuf), L"cal.py", true)) {
        _wprintf_e(L"Failed to find cal.py\n");
        ExitProcess(1);
    }

    size_t file_len = wcslen(filebuf);

    wchar_t* cmd = Mem_alloc((len + file_len + 15) * sizeof(wchar_t));
    if (cmd == NULL) {
        ExitProcess(1);
    }

    memcpy(cmd, L"python.exe -x ", 14 * sizeof(wchar_t));
    memcpy(cmd + 14, filebuf, file_len * sizeof(wchar_t));
    memcpy(cmd + 14 + file_len, arg, (len + 1) * sizeof(wchar_t));

    unsigned long status;
    if (!subprocess_run(cmd, NULL, INFINITE, &status, SUBPROCESS_STDIN_INHERIT | SUBPROCESS_STDOUT_INHERIT)) {
        _wprintf_e(L"Failed to run cal.py\n");
        ExitProcess(1);
    }
    ExitProcess(status);
}
