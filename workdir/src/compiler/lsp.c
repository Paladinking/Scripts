#include "log.h"
#include "args.h"
#include "dynamic_string.h"
#include "printf.h"


int lsp(char** argv, int argc) {
    FlagValue port = {FLAG_INT | FLAG_OPTONAL_VALUE};
    FlagValue host = {FLAG_STRING | FLAG_OPTONAL_VALUE};

    FlagInfo flags[] = {
        {'s', "log-to-socket", NULL}, // 0
        {'\0', "tcp"}, // 1
        {'\0', "port", &port}, // 2
        {'\0', "host", &host}, // 3
    };

    const uint32_t flag_count = sizeof(flags) / sizeof(FlagInfo);
    ErrorInfo err;
    if (!find_flags(argv, &argc, flags, flag_count, &err)) {
        char* s = format_error(&err, flags, flag_count);
        if (s != NULL) {
            _printf_e("%s\n", s);
            Mem_free(s);
        }
        return 1;
    }

    bool log_to_socket = flags[0].count > 0;

    Log_Init(log_to_socket);

    oprintf("Hello world %d", argc);

    return 0;
}

#ifdef WIN32
int main() {
    // Obtain utf8 command line
    wchar_t* cmd = GetCommandLineW();
    String argbuf;
    String_create(&argbuf);
    if (!String_from_utf16_str(&argbuf, cmd)) {
        return 1;
    }
    int argc;
    char** argv = parse_command_line(argbuf.buffer, &argc);
    String_free(&argbuf);
    int status = lsp(argv, argc);
    Log_Shutdown();
    Mem_free(argv);

    ExitProcess(status);
}

#else
int main(char** argv, int argc) {
    Log_Init();
    int status = compiler(argv, argc);
    Log_Shutdown();
    return status;
}
#endif
