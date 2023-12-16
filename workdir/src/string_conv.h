#include <windows.h>

LPSTR WideStringToNarrow(LPCWSTR string, DWORD *size);

LPWSTR NarrowStringToWide(LPCSTR string, DWORD *size);
