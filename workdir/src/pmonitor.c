#include <windows.h>
#include <winternl.h>
#include "printf.h"
#include "mem.h"

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



int main() {
    HANDLE job = CreateJobObjectW(NULL, NULL);

    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);

    JOBOBJECT_ASSOCIATE_COMPLETION_PORT port;
    ZeroMemory(&port, sizeof(port));
    port.CompletionKey = job;
    port.CompletionPort = iocp;

    SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation, &port, sizeof(port));

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    wchar_t cmd[] = L"ps\0";
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        _wprintf_e(L"Failed to create process\n");
        return 0;
    }

    Sleep(5000);
    AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);

    DWORD msg;
    ULONG_PTR key;
    LPOVERLAPPED ov;

    while (1) {
        GetQueuedCompletionStatus(iocp, &msg, &key, &ov, INFINITE);
        switch (msg) {
            case JOB_OBJECT_MSG_NEW_PROCESS: {
                DWORD pid = ((ULONG_PTR)ov);
                wchar_t* cmd = GetCommandLineFromPid(pid);
                _wprintf(L"New process %u, %s\n", pid, cmd);
                break;
            }
            case JOB_OBJECT_MSG_EXIT_PROCESS: {
                DWORD pid = ((ULONG_PTR)ov);
                _printf("exit process %u\n", pid);
                break;
            }
        }
    }
}
