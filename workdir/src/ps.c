#include "subprocess.h"
#include "printf.h"

#include <windows.h>
#include <tlhelp32.h>
#include <winternl.h>

typedef NTSTATUS (NTAPI *NtQueryInformationProcess_t)(
    HANDLE,
    PROCESSINFOCLASS,
    PVOID,
    ULONG,
    PULONG
);

wchar_t* GetCommandLineFromPid(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                  FALSE, pid);
    if (!hProcess) {
        return NULL;
    }

    NtQueryInformationProcess_t NtQueryInformationProcess =
        (NtQueryInformationProcess_t)GetProcAddress(
            GetModuleHandleW(L"ntdll.dll"),
            "NtQueryInformationProcess"
        );

    ULONG size = 0;
    NtQueryInformationProcess(
        hProcess,
        60,
        NULL,
        0,
        &size
    );


    BYTE* buffer = Mem_alloc(size);

    if (!NT_SUCCESS(NtQueryInformationProcess(
        hProcess,
        60,
        buffer,
        size,
        &size))) {
        CloseHandle(hProcess);
        return NULL;
    }

    PUNICODE_STRING cmd = (PUNICODE_STRING)(buffer);

    wchar_t* res = Mem_alloc((cmd->Length + 1) * sizeof(wchar_t));
    memcpy(res, cmd->Buffer, cmd->Length * sizeof(wchar_t));
    res[cmd->Length] = L'\0';
    Mem_free(buffer);

    CloseHandle(hProcess);
    return res;
}


int iter() {
    // Create a snapshot of the system processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return -1;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    _wprintf(L"PID\tParent PID\tCMD\n");
    if (Process32First(hSnapshot, &pe32)) {
        do {
            wchar_t* cmd = GetCommandLineFromPid(pe32.th32ProcessID);
            if (cmd != NULL) {
                _wprintf(L"%lu\t%lu\t\t%s\n", pe32.th32ProcessID, pe32.th32ParentProcessID, cmd);
            }
            Mem_free(cmd);
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return 0;
}



int main() {
    return iter();
}
