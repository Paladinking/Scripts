#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include "printf.h"





__declspec(dllexport) DWORD test() {
    _printf("Hello world\n");

    return 0;
}



BOOL DLLMain() {



    return TRUE;
}
