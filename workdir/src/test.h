#pragma once
#define FAIL_TEST_S(n, i, s, l) WriteConsoleA(handle, "Failed ", 7, 0, 0); \
		WriteConsoleA(handle, TEST_NAMES[n], TEST_NAME_SIZE[n], 0, 0); \
		WriteConsoleA(handle, " "#i": ", sizeof(#i) + 2, 0, 0); \
		WriteConsoleA(handle, s, l, 0, 0); return 1
#define FAIL_TEST(n, i, s) FAIL_TEST_S(n, i, s, sizeof(s))

#define SUCCES_TEST(n) WriteConsoleA(handle, "Passed ", 7, 0, 0); \
		WriteConsoleA(handle, TEST_NAMES[n], TEST_NAME_SIZE[n], 0, 0); \
		WriteConsoleA(handle, "\n", 1, 0, 0)


extern unsigned GetStdHandle(unsigned);
extern int WriteConsoleA(unsigned, const char*, unsigned, unsigned*, void*);
extern unsigned GetFullPathNameA(const char*, unsigned, char*, char**);

typedef unsigned long long size_t;

extern size_t strlen(const char* buf);
extern void memcopy(const void* src, void* dest, int len);
extern int strcmp(const char* a, const char* b);

extern unsigned parse_args(char** buffer, int len);