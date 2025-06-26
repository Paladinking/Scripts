#ifndef OCHAR_H_00
#define OCHAR_H_00
#include <stdint.h>

#ifndef NARROW_OCHAR
    typedef wchar_t ochar_t;
    #define oL(s) L##s
    
    #define parse_uintw(s, i, base) parse_uint(s, i, base)
    #define parse_sintw(s, i, base) parse_sint(s, i, base)
    
    #define ostrcmp wcscmp
    #define ostrlen wcslen
    #define ostrchr wcschr
    #define ostrncmp wcsncmp
#else
    typedef char ochar_t;
    #define oL(s) s

    #define ostrcmp strcmp
    #define ostrlen strlen
    #define ostrchr strchr
    #define ostrncmp strncmp
#endif


#endif
