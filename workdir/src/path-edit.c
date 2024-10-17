#define UNICODE
#include "cli.h"
#include "printf.h"

int main() {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));

    wchar_t argv[] = L"vim.exe -c \"set visualbell | set t_vb=\"\0";

    if (!CreateProcessW(NULL, argv, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        _wprintf(L"Failed creating process: %d\n", GetLastError());
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}
