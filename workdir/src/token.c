#define UNICODE
#include <windows.h>
#include "printf.h"
#include "args.h"







int main() {
    int argc;
    LPWSTR* argv = parse_command_line(GetCommandLine(), &argc);


    _printf("Hello world\n");

    return 0;
}

