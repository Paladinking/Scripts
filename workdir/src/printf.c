#define UNICODE
#include "printf.h"
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
extern int _vscprintf(const char *format, va_list argptr);
extern int _vscwprintf(const wchar_t *format,va_list argptr);
extern int _vsnprintf(char *buffer, size_t count, const char *format, va_list argptr);
extern int _vsnwprintf(wchar_t *buffer, size_t count, const wchar_t *format, va_list argptr);
#ifdef __cplusplus
}
#endif

int _printf_h(HANDLE dest, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int count = _vscprintf(fmt, args);
	if (count == -1) {
		return -1;
	}
	if (count < 1024) { // Note: not inclusive for NULL terminator
		char buf[1024];
		_vsnprintf(buf, count, fmt, args);
		WriteConsoleA(dest, buf, count, NULL, NULL);
	} else {
		HANDLE heap = GetProcessHeap();
		char *buf = (char*) HeapAlloc(heap, 0, count);
		if (buf == NULL) {
			return -1;
		}
		_vsnprintf(buf, count, fmt, args);
		WriteConsoleA(dest, buf, count, NULL, NULL);
		HeapFree(heap, 0, buf);
	}
	va_end(args);
	return count;
}

int _wprintf_h(HANDLE dest, const wchar_t* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int count = _vscwprintf(fmt, args);
	if (count == -1) {
		return -1;
	}
	if (count < 512) { // Note: not inclusive for NULL terminator
		wchar_t buf[512];
		_vsnwprintf(buf, count, fmt, args);
		WriteConsoleW(dest, buf, count, NULL, NULL);
	} else {
		HANDLE heap = GetProcessHeap();
		wchar_t *buf = (wchar_t*) HeapAlloc(heap, 0, count * sizeof(wchar_t));
		if (buf == NULL) {
			return -1;
		}
		_vsnwprintf(buf, count, fmt, args);
		WriteConsoleW(dest, buf, count, NULL, NULL);
		HeapFree(heap, 0, buf);
	}
	va_end(args);
	return count;
}
