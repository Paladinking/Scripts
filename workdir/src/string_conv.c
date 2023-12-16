#define UNICODE
#include "string_conv.h"

LPWSTR NarrowStringToWide(LPCSTR string, DWORD *size) {
	HANDLE heap = GetProcessHeap();
	UINT code_point = 65001; // Utf-8
	DWORD out_req_size = MultiByteToWideChar(
		code_point, 0, string, *size, NULL, 0 
	);

	LPWSTR buffer = HeapAlloc(heap, 0, (out_req_size + 1) * sizeof(WCHAR));
	if (buffer == NULL) {
		return NULL;
	}
	out_req_size = MultiByteToWideChar(
		code_point, 0, string, *size, buffer, out_req_size 
	);

	if (out_req_size == 0) {
		HeapFree(heap, 0, buffer);
		return NULL;
	}
	*size = out_req_size;
	buffer[out_req_size] = L'\0';
	return buffer;
}

LPSTR WideStringToNarrow(LPCWSTR string, DWORD *size) {
	HANDLE heap = GetProcessHeap();
	UINT code_point = 65001; // Utf-8
	DWORD out_req_size = WideCharToMultiByte(
		code_point, 0, string, *size, NULL, 0, NULL, NULL 
	);

	LPSTR buffer = HeapAlloc(heap, 0, out_req_size + 1);
	if (buffer == NULL) {
		return NULL;
	}
	out_req_size = WideCharToMultiByte(
		code_point, 0, string, *size, buffer, out_req_size, NULL, NULL 
	);

	if (out_req_size == 0) {
		HeapFree(heap, 0, buffer);
		return NULL;
	}
	*size = out_req_size;
	buffer[out_req_size] = '\0';
	return buffer;
}