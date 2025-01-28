#include "subprocess.h"
#define WIN32_LEAN_AND_MEAN
#include "mem.h"
#include <stdint.h>
#include <windows.h>

bool subprocess_run(const wchar_t *cmd, String *out,
                    unsigned int timeout_millies, unsigned long *exit_code,
                    unsigned opts) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));

    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, NULL, 0)) {
        return false;
    }
    if (!SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT,
                              HANDLE_FLAG_INHERIT)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }

    DWORD flags = 0;

    HANDLE in = INVALID_HANDLE_VALUE;
    if (opts & SUBPROCESS_STDIN_DEVNULL) {
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.lpSecurityDescriptor = NULL;
        attr.bInheritHandle = TRUE;
        in = CreateFileW(L"NUL", GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, &attr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in;
    if (opts & SUBPROCESS_STDERR_NONE) {
        flags = CREATE_NO_WINDOW;
        si.hStdError = INVALID_HANDLE_VALUE;
    } else {
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }
    si.hStdOutput = hWrite;

    size_t cmd_len = wcslen(cmd);
    wchar_t *cmd_buf = Mem_alloc((cmd_len + 1) * sizeof(wchar_t));
    if (cmd_buf == NULL) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }
    memcpy(cmd_buf, cmd, (cmd_len + 1) * sizeof(wchar_t));

    if (!CreateProcessW(NULL, cmd_buf, NULL, NULL, TRUE, flags, NULL, NULL, &si,
                        &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        Mem_free(cmd_buf);
        return false;
    }
    CloseHandle(hWrite);
    CloseHandle(pi.hThread);
    if (in != INVALID_HANDLE_VALUE) {
        CloseHandle(in);
    }

    bool status = false;

    OVERLAPPED o;
    o.Offset = 0;
    o.OffsetHigh = 0;
    o.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!o.hEvent) {
        goto cleanup;
    }
    DWORD count;

    LARGE_INTEGER stamp, freq;
    if (!QueryPerformanceCounter(&stamp) || !QueryPerformanceFrequency(&freq)) {
        goto cleanup;
    }
    uint64_t stamp_millies = (stamp.QuadPart * 1000) / freq.QuadPart;

    while (1) {
        if (!String_reserve(out, out->length + 1025)) {
            goto cleanup;
        }
        if (!ReadFile(hRead, out->buffer + out->length, 1024, &count, &o)) {
            if (GetLastError() != ERROR_IO_PENDING) {
                break;
            }
        }
        if (!QueryPerformanceCounter(&stamp)) {
            goto cleanup;
        }
        uint64_t now = (stamp.QuadPart * 1000) / freq.QuadPart;
        uint64_t delta = now - stamp_millies;
        if (delta > timeout_millies) {
            goto cleanup;
        }
        if (!GetOverlappedResultEx(hRead, &o, &count, timeout_millies - delta,
                                   FALSE)) {
            goto cleanup;
        }
        out->length += count;
    }

    uint64_t now = (stamp.QuadPart * 1000) / freq.QuadPart;
    uint64_t delta = now - stamp_millies;
    out->buffer[out->length] = '\0';
    if (delta <= timeout_millies) {
        if (WaitForSingleObject(pi.hProcess, timeout_millies - delta) !=
            WAIT_TIMEOUT) {
            GetExitCodeProcess(pi.hProcess, exit_code);
            CloseHandle(pi.hProcess);
            pi.hProcess = NULL;
        }
    }
    status = true;
cleanup:
    if (pi.hProcess) {
        *exit_code = STATUS_TIMEOUT;
        TerminateProcess(pi.hProcess, STATUS_TIMEOUT);
        CloseHandle(pi.hProcess);
    }
    if (hRead) {
        CloseHandle(hRead);
    }
    if (o.hEvent) {
        CloseHandle(o.hEvent);
    }
    Mem_free(cmd_buf);
    return status;
}
