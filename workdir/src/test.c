#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#include <windows.h>
#include "args.h"
#include "regex.h"
#include "glob.h"
#include "printf.h"



int main() {
    /*Regex* a = Regex_compile("hello world");
    _printf("%u, %u, %u\n", Regex_fullmatch(a, "hello world", 11), 
                            Regex_fullmatch(a, "hello world2", 12), Regex_fullmatch(a, "", 0));

    Regex* b = Regex_compile("");
    _printf("%u, %u, %u\n", Regex_fullmatch(b, "hello world", 11), 
                            Regex_fullmatch(b, "hello world2", 12), Regex_fullmatch(b, "", 0));
    Regex* c = Regex_compile("a*..b*");
    _printf("%u, %u, %u\n", Regex_fullmatch(c, "hello world", 11), 
                            Regex_fullmatch(c, "hello world2", 12), Regex_fullmatch(c, "", 0));
    _printf("%u, %u, %u\n", Regex_fullmatch(c, "12", 2), 
                            Regex_fullmatch(c, "aaaaaaaaaabb", 12), Regex_fullmatch(c, "ab", 2));
    _printf("%u, %u, %u\n", Regex_fullmatch(c, "b12a", 4), 
                            Regex_fullmatch(c, "aaa12bbbb", 9), Regex_fullmatch(c, "aaa12bbbc", 9));

    const char* r = "This* is* \\(ab.-*c\\)* a test";
    Regex* d = Regex_compile(r);
*/
    WString buf;
    WString_create_capacity(&buf, 4096);
    String s;
    String_create(&s);

    _printf("Enter regex: ");
    DWORD read;
    ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buf.buffer, 4096, &read, NULL);
    String_from_utf16_bytes(&s, buf.buffer, read);
    while (s.length > 0 && s.buffer[s.length - 1] == '\n' || s.buffer[s.length - 1] == '\r') {
        s.length -= 1;
        s.buffer[s.length] = '\0';
    }
    Regex* e = Regex_compile(s.buffer);
    if (e == NULL) {
        _printf("Failed parsing regex\n");
        return 1;
    }
    _printf("Compiled regex: '%s'\n", s.buffer);

    while (1) {
        ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buf.buffer, 4096, &read, NULL);
        String_from_utf16_bytes(&s, buf.buffer, read);
        while (s.length > 0 && s.buffer[s.length - 1] == '\n' || s.buffer[s.length - 1] == '\r') {
            s.length -= 1;
            s.buffer[s.length] = '\0';
        }
        if (strcmp(s.buffer, "exit") == 0) {
            break;
        }
        _printf("'%s' match: %u\n", s.buffer, Regex_fullmatch(e, s.buffer, s.length));
    }


    return 0;
}
