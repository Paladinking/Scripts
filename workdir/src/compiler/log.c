#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <Winsock2.h>
#include <windows.h>
#include "log.h"
#include <printf.h>

const char* LOG_FORMAT = "{\"name\":\"%s\",\"level\":\"%s\",\"pathname\":\"%s\",\"lineno\":\"%d\",\"thread\":%lu,\"msg\":\"";

const char* PRIORITIES[] = {"DEBUG","INFO","WARNING","ERROR","CRITICAL"};
const char* CATAGORIES[LOG_CATAGORY_MAX] = {
    "User", "Tables", "Tokenizer", "Scanner", "Parser", "Type Checker",
    "Quads Generator", "Register Allocator", "Assembly Generator",
    "Assembler", "Object Parser", "Linker", "Object Writer"
};

const enum LogLevel MIN_LEVEL[LOG_CATAGORY_MAX] = {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_WARNING
};

SRWLOCK log_lock;
SOCKET log_socket = INVALID_SOCKET;

void Log_Init(bool log_to_socket) {
    if (!log_to_socket) {
        return;
    }
    InitializeSRWLock(&log_lock);

    WORD versionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(versionRequested, &wsaData)) {
        _wprintf_e(L"Failed initializing sockets\n");
        return;
    }

    log_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (log_socket == INVALID_SOCKET) {
        _wprintf_e(L"Failed creating socket: %lu\n", WSAGetLastError());
    }

    u_long val = 1;
    int res = ioctlsocket(log_socket, FIONBIO, &val);
    if (res != 0) {
        closesocket(log_socket);
        log_socket = INVALID_SOCKET;
        return;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;

    sin.sin_port = htons(19996);
    sin.sin_addr.s_addr = htonl(0x7f000001);

    res = connect(log_socket, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
    if (res != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
        _wprintf_e(L"Failed connecting socket\n");
        closesocket(log_socket);
        WSACleanup();
        log_socket = INVALID_SOCKET;
        return;
    }

    fd_set write;
    FD_ZERO(&write);
    FD_SET(log_socket, &write);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    res = select(log_socket + 1, NULL, &write, NULL, &timeout);
    if (res != 1) {
        _wprintf_e(L"Failed connecting socket\n");
        closesocket(log_socket);
        WSACleanup();
        log_socket = INVALID_SOCKET;
        return;
    }

    LOG_USER_DEBUG("Logging initialized");
}

void Log_Shutdown() {
    if (log_socket == INVALID_SOCKET) {
        return;
    }
    shutdown(log_socket, SD_SEND);

    closesocket(log_socket);

    WSACleanup();

    log_socket = INVALID_SOCKET;
}

void Log_LogStrAtLevel(enum LogCatagory catagory, enum LogLevel level,
                       const char* file, int32_t line,
                       const char* str, uint32_t len) {
    if (level >= MIN_LEVEL[catagory]) {
        outputUtf8_e(str, len);
    }

    if (log_socket == INVALID_SOCKET) {
        return;
    }

    const char* prio = PRIORITIES[level];

    DWORD id = GetCurrentThreadId();

    char filename[513];
    char modname[513];
    int name_ix = 0, offset = 0, mod_ix = 0;
    while (file[offset] != '\0' && name_ix < 512) {
        if (file[offset] == '\\') {
            filename[name_ix++] = '\\';
            filename[name_ix++] = '\\';
            modname[mod_ix++] = '.';
        } else {
            filename[name_ix++] = file[offset];
            if (file[offset] == '/') {
                modname[mod_ix++] = '.';
            } else if (file[offset] == '.') {
                modname[mod_ix++] = '_';
            } else {
                modname[mod_ix++] = file[offset];
            }
        }
        ++offset;
    }
    filename[name_ix] = '\0';
    modname[mod_ix] = '\0';

    char args_buf[2053];
    int size = _snprintf_s(args_buf + 4, 2048, _TRUNCATE, LOG_FORMAT, modname, prio, filename, line, id);
    if (size < 0) {
        return;
    }

    if (size + 6 + len > 2052) {
        len = 2052 - size - 6;
    }

    memcpy(args_buf + size + 4, str, len);

    size = size + len + 2;

    uint32_t s = size;
    args_buf[0] = 0;
    args_buf[1] = 0;
    args_buf[2] = (s >> 8) & 0xff;
    args_buf[3] = s & 0xff;
    args_buf[4 + size - 2] = '"';
    args_buf[4 + size - 1] = '}';
    args_buf[4 + size] = '\0';

    AcquireSRWLockExclusive(&log_lock);

    if (log_socket == INVALID_SOCKET) {
        ReleaseSRWLockExclusive(&log_lock);
        return;
    }

    fd_set write;
    FD_ZERO(&write);
    FD_SET(log_socket, &write);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;

    int res = select(log_socket + 1, NULL, &write, NULL, &timeout);
    if (res != 1) {
        ReleaseSRWLockExclusive(&log_lock);
        return;
    }

    res = send(log_socket, args_buf, size + 4, 0);

    ReleaseSRWLockExclusive(&log_lock);
}

void Log_LogAtLevel(enum LogCatagory catagory, enum LogLevel level, const char* file, int32_t line, const char* fmt, ...) {

    va_list args;
    va_start(args, fmt);
    char args_buf[2052];
    int size = 0;

    if (log_socket != INVALID_SOCKET) {
        const char* prio = PRIORITIES[level];

        DWORD id = GetCurrentThreadId();

        char filename[513];
        char modname[513];
        int name_ix = 0, offset = 0, mod_ix = 0;
        while (file[offset] != '\0' && name_ix < 512) {
            if (file[offset] == '\\') {
                filename[name_ix++] = '\\';
                filename[name_ix++] = '\\';
                modname[mod_ix++] = '.';
            } else {
                filename[name_ix++] = file[offset];
                if (file[offset] == '/') {
                    modname[mod_ix++] = '.';
                } else if (file[offset] == '.') {
                    modname[mod_ix++] = '_';
                } else {
                    modname[mod_ix++] = file[offset];
                }
            }
            ++offset;
        }
        filename[name_ix] = '\0';
        modname[mod_ix] = '\0';

        size = _snprintf_s(args_buf + 4, 2048, _TRUNCATE, LOG_FORMAT, modname, 
                           prio, filename, line, id);
        if (size < 0) {
            return;
        }
    }

    int fm_size = _vsnprintf_s(args_buf + size + 4, 2048 - size - 2, _TRUNCATE, fmt, args);
    if (fm_size < 0) {
        fm_size = 2047 - size - 2;
        if (level >= MIN_LEVEL[catagory]) {
            args_buf[size + 4 + fm_size] = '\n';
            outputUtf8_e(args_buf + size + 4, fm_size + 1);
        }
        size = 2047;
    } else {
        if (level >= MIN_LEVEL[catagory]) {
            args_buf[size + 4 + fm_size] = '\n';
            outputUtf8_e(args_buf + size + 4, fm_size + 1);
        }
        size = size + fm_size + 2;
    }
    if (log_socket == INVALID_SOCKET) {
        return;
    }

    uint32_t s = size;
    args_buf[0] = 0;
    args_buf[1] = 0;
    args_buf[2] = (s >> 8) & 0xff;
    args_buf[3] = s & 0xff;
    args_buf[4 + size - 2] = '"';
    args_buf[4 + size - 1] = '}';
    args_buf[4 + size] = '\0';

    AcquireSRWLockExclusive(&log_lock);

    if (log_socket == INVALID_SOCKET) {
        ReleaseSRWLockExclusive(&log_lock);
        return;
    }

    fd_set write;
    FD_ZERO(&write);
    FD_SET(log_socket, &write);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;

    int res = select(log_socket + 1, NULL, &write, NULL, &timeout);
    if (res != 1) {
        ReleaseSRWLockExclusive(&log_lock);
        return;
    }

    res = send(log_socket, args_buf, size + 4, 0);

    ReleaseSRWLockExclusive(&log_lock);

    va_end(args);
}
