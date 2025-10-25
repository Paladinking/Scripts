#ifndef OCHAR_H_00
#define OCHAR_H_00
#include <stdint.h>
#include <wchar.h>

#ifndef NARROW_OCHAR
    typedef wchar_t ochar_t;
    #define oL(s) L##s
    
    #define parse_uintw(s, i, base) parse_uint(s, i, base)
    #define parse_sintw(s, i, base) parse_sint(s, i, base)
    
    #define ostrcmp wcscmp
    #define ostricmp _wcsicmp
    #define ostrlen wcslen
    #define ostrchr wcschr
    #define ostrrchr wcsrchr
    #define ostrncmp wcsncmp
    #define otoupper towupper

    #define OString WString
    #define OString_noinit WString_noinit
    #define OString_create WString_create
    #define OString_create_capacity WString_create_capacity
    #define OString_free WString_free
    #define OString_append WString_append
    #define OString_append_count WString_append_count
#else
    typedef char ochar_t;
    #define oL(s) s

    #define ostrcmp strcmp
    #define ostricmp _stricmp
    #define ostrlen strlen
    #define ostrchr strchr
    #define ostrrchr strrchr
    #define ostrncmp strncmp
    #define otoupper toupper

    #define OString String
    #define OString_noinit String_noinit
    #define OString_create String_create
    #define OString_create_capacity String_create_capacity
    #define OString_free String_free
    #define OString_append String_append
    #define OString_append_count String_append_count
#endif


#endif
