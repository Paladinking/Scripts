#define UNICODE
#include "string_conv.h"

BOOL NarrowStringToWide(LPCSTR string, DWORD size, WideBuffer *buffer) {
    HANDLE heap = GetProcessHeap();
    UINT code_point = 65001; // Utf-8
    DWORD out_req_size =
        MultiByteToWideChar(code_point, 0, string, size, NULL, 0);
    if (buffer->ptr == NULL || buffer->capacity < (out_req_size + 1)) {
        HeapFree(heap, 0, buffer->ptr);
        buffer->ptr = HeapAlloc(heap, 0, (out_req_size + 1) * sizeof(WCHAR));
        if (buffer->ptr == NULL) {
            buffer->size = 0;
            buffer->capacity = 0;
            return FALSE;
        }
        buffer->capacity = out_req_size + 1;
        buffer->size = out_req_size;
    }

    out_req_size = MultiByteToWideChar(code_point, 0, string, size, buffer->ptr,
                                       out_req_size);

    if (out_req_size == 0) {
        HeapFree(heap, 0, buffer->ptr);
        buffer->capacity = 0;
        buffer->size = 0;
        return FALSE;
    }
    buffer->size = out_req_size;

    buffer->ptr[out_req_size] = L'\0';
    return TRUE;
}

BOOL WideStringToNarrow(LPCWSTR string, DWORD size, StringBuffer *buffer) {
    HANDLE heap = GetProcessHeap();
    UINT code_point = 65001; // Utf-8
    DWORD out_req_size =
        WideCharToMultiByte(code_point, 0, string, size, NULL, 0, NULL, NULL);

    if (buffer->ptr == NULL || buffer->capacity < (out_req_size + 1)) {
        HeapFree(heap, 0, buffer->ptr);
        buffer->ptr = HeapAlloc(heap, 0, (out_req_size + 1));
        if (buffer->ptr == NULL) {
            buffer->size = 0;
            buffer->capacity = 0;
            return FALSE;
        }
        buffer->capacity = out_req_size + 1;
        buffer->size = out_req_size;
    }

    out_req_size = WideCharToMultiByte(code_point, 0, string, size, buffer->ptr,
                                       out_req_size, NULL, NULL);

    if (out_req_size == 0) {
        HeapFree(heap, 0, buffer->ptr);
        buffer->size = 0;
        buffer->capacity = 0;
        return FALSE;
    }
    buffer->size = out_req_size;
    buffer->ptr[out_req_size] = '\0';
    return TRUE;
}
