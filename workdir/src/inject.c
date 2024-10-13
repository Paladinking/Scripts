#define UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include "printf.h"

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
    DWORD parent = GetParentProcessId();
    if (parent == 0) {
        _printf("Could not get parent process id\n");
        return 1;
    }

    wchar_t modbuf[1025];
    DWORD res = GetModuleFileName(NULL, modbuf, 1024);
    if (res == 0 || res >= 1024) {
        _printf("Failed getting module path\n");
        return 1;
    }
    wchar_t drive[10], dir[1024];
    if (_wsplitpath_s(modbuf, drive, 10, dir, 1024, NULL, 0, NULL, 0) != 0) {
        _printf("Failed getting directory\n");
        return 1;
    }
    if (_wmakepath_s(modbuf, 1024, drive, dir, L"testdll", L"dll") != 0) {
        _printf("Failed getting directory\n");
        return 1;
    }

    DWORD attr = GetFileAttributesW(modbuf);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        _printf("File testdll.dll does not exits\n");
        return 1;
    }

    _wprintf(L"Found dll at %s\n", modbuf);

    LPVOID loadLibararyAddr = (LPVOID) GetProcAddress(
            GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");

    HANDLE hParent = OpenProcess(PROCESS_ALL_ACCESS, FALSE, parent);
    if (hParent == NULL) {
        _printf("Could not open parent process handle\n");
        return 1;
    }

    size_t len = (wcslen(modbuf) + 1) * sizeof(wchar_t);

    HMODULE dll = LoadLibraryW(modbuf);
    if (dll == NULL) {
        _wprintf(L"Failed loading library %s\n", modbuf);
        CloseHandle(hParent);
        return 1;
    }

    LPVOID entry = GetProcAddress(dll, "entry");
    if (entry == NULL) {
        _printf("Failed getting dll entry function\n");
        FreeLibrary(dll);
        CloseHandle(hParent);
        return 1;
    }

    wchar_t* name = VirtualAllocEx(hParent, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (name == NULL || !WriteProcessMemory(hParent, name, modbuf, len, NULL)) {
        _printf("Failed allocating path buffer\n");
        FreeLibrary(dll);
        CloseHandle(hParent);
        return 1;
    }

    HANDLE hThread;
    if (!(hThread = CreateRemoteThread(hParent, NULL, 0, loadLibararyAddr, name, 0, NULL))) {
        _printf("Failed creating remote thread\n");
        VirtualFreeEx(hParent, name, 0, MEM_RELEASE);
        FreeLibrary(dll);
        CloseHandle(hParent);
        return 1;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hParent, name, 0, MEM_RELEASE);

    if (!(hThread = CreateRemoteThread(hParent, NULL, 0, entry, NULL, 0, NULL))) {
        _printf("Failed executing thread entry\n");
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
