#define UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include "printf.h"
#include "args.h"
#include "glob.h"

DWORD GetParentProcessId() {
    // Get the current process ID
    DWORD currentProcessId = GetCurrentProcessId();

    // Create a snapshot of the system processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Iterate through the processes to find the parent process
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == currentProcessId) {
                // Found the current process in the snapshot, return its parent process ID
                CloseHandle(hSnapshot);
                return pe32.th32ParentProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    // Process not found or unable to retrieve information
    CloseHandle(hSnapshot);
    return 0;
}

unsigned char func[] = {83, 85, 72, 131, 236, 72, 72, 137, 203, 72, 141, 75, 16, 255, 19, 72, 137, 197, 72, 137, 193, 72, 199, 68, 36, 40, 116, 101, 115, 116, 72, 141, 84, 36, 40, 255, 83, 8, 72, 137, 233, 255, 208, 72, 131, 196, 72, 93, 91, 72, 49, 192, 195};


int main() {
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    DWORD parent = GetParentProcessId();
    if (parent == 0) {
        _printf_h(err, "Could not get parent process id\n");
        return 1;
    }

    int argc;
    wchar_t** argv = parse_command_line(GetCommandLineW(), &argc);

    bool substitute = find_flag(argv, &argc, L"-s", L"--substitute") > 0;
    bool verbose = find_flag(argv, &argc, L"-v", L"--verbose") > 0;

    wchar_t modbuf[1025];
    if (!find_file_relative(modbuf, 1024, L"autocmp.dll", true)) {
        _printf_h(err, "Could not find autocmp.dll\n");
        return 1;
    }
    if (verbose) {
        _wprintf(L"Found dll at %s\n", modbuf);
    }

    LPVOID loadLibararyAddr = (LPVOID) GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");

    HANDLE hParent = OpenProcess(PROCESS_ALL_ACCESS, FALSE, parent);
    if (hParent == NULL) {
        _printf_h(err, "Could not open parent process handle\n");
        return 1;
    }

    size_t len = (wcslen(modbuf) + 1) * sizeof(wchar_t);

    HMODULE dll = LoadLibraryW(modbuf);
    if (dll == NULL) {
        _wprintf_h(err, L"Failed loading library %s\n", modbuf);
        CloseHandle(hParent);
        return 1;
    }

    const char* entry_point = substitute ? "reload_with_cmd" : "reload";
    LPVOID entry = GetProcAddress(dll, entry_point);
    if (entry == NULL) {
        _printf_h(err, "Failed getting dll entry function\n");
        FreeLibrary(dll);
        CloseHandle(hParent);
        return 1;
    }

    wchar_t* name = VirtualAllocEx(hParent, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (name == NULL || !WriteProcessMemory(hParent, name, modbuf, len, NULL)) {
        _printf_h(err, "Failed allocating path buffer\n");
        FreeLibrary(dll);
        CloseHandle(hParent);
        return 1;
    }

    HANDLE hThread;
    if (!(hThread = CreateRemoteThread(hParent, NULL, 0, loadLibararyAddr, name, 0, NULL))) {
        _printf_h(err, "Failed creating remote thread\n");
        VirtualFreeEx(hParent, name, 0, MEM_RELEASE);
        FreeLibrary(dll);
        CloseHandle(hParent);
        return 1;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hParent, name, 0, MEM_RELEASE);

    if (!(hThread = CreateRemoteThread(hParent, NULL, 0, entry, NULL, 0, NULL))) {
        _printf_h(err, "Failed executing thread entry\n");
        FreeLibrary(dll);
        CloseHandle(hParent);
        return 1;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    VirtualFreeEx(hParent, name, 0, MEM_RELEASE);
    FreeLibrary(dll);
    CloseHandle(hParent);
    return 0;
}
