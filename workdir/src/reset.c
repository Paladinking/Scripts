#define WIN32_LEAN_AND_MEAN
#include <windows.h>    




int main() {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetFileType(out) != FILE_TYPE_CHAR) {
        ExitProcess(1);
    }

    DWORD mode;
    if (!GetConsoleMode(out, &mode) || !SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        ExitProcess(1);
    }

    WriteConsoleW(out, L"\x1B" L"c\x1B[!p", 7, NULL, NULL);

    SetConsoleMode(out, mode);

    ExitProcess(0);
}
