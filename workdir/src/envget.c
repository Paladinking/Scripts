#define UNICODE
#include "mem.h"
#include <UserEnv.h>
#include <windows.h>

int main() {
    LPWSTR env;

    LPSTR buffer = Mem_alloc(4096);
    DWORD buffer_size = 4096;
    LPWSTR expanded = Mem_alloc(4096);
    DWORD expanded_size = 4096;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

    HANDLE user;
    HANDLE process = GetCurrentProcess();
    OpenProcessToken(process, TOKEN_QUERY | TOKEN_DUPLICATE, &user);

    CreateEnvironmentBlock(&env, user, FALSE);
    SetEnvironmentStringsW(env);

    DWORD index = 0;
    while (1) {
        int size = 0;
        while (env[index + size++]) {
        }
        DWORD req_exp_size = ExpandEnvironmentStrings(
            env + index, expanded, expanded_size / sizeof(WCHAR));
        if (req_exp_size * sizeof(WCHAR) > expanded_size) {
            Mem_free(expanded);
            expanded_size = req_exp_size * sizeof(WCHAR);
            expanded = Mem_alloc(expanded_size);
            ExpandEnvironmentStrings(env + index, expanded, req_exp_size);
        }

        DWORD req_size = WideCharToMultiByte(CP_UTF8, 0, expanded, -1, buffer,
                                             buffer_size, NULL, NULL);
        if (req_size > buffer_size) {
            Mem_free(buffer);
            buffer_size = req_size;
            buffer = Mem_alloc(buffer_size);
        }
        WideCharToMultiByte(CP_UTF8, 0, expanded, -1, buffer, buffer_size, NULL,
                            NULL);
        WriteFile(out, buffer, req_size - 1, NULL, NULL);
        WriteFile(out, "\n", 1, NULL, NULL);
        index = index + size;
        if (!env[index])
            break;
    }

    FlushFileBuffers(out);
    Mem_free(buffer);
    DestroyEnvironmentBlock(env);
    return 0;
}
