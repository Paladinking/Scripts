#define UNICODE
#include <windows.h>
#include "printf.h"
#include "dynamic_string.h"

int x = 50;
unsigned cursor = 0;

DynamicWString data;

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            HDC dc = CreateCompatibleDC(hdc);
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            _printf("%d %d %d %d\n", clientRect.left, clientRect.top, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

            HBITMAP bmp = CreateCompatibleBitmap(hdc, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
            HBITMAP old_bmp = (HBITMAP) SelectObject(dc, bmp);
            FillRect(dc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            RECT r;
            r.top = x;
            r.left = 50;
            r.right = 250;
            r.bottom = 200 + x;
            FillRect(dc, &r, (HBRUSH)COLOR_DESKTOP+1);
            r.top = x;
            r.left = 50;
            r.right = 250;
            r.bottom = 200 + x;
            SetBkMode(dc, TRANSPARENT);
    //        DrawText(dc, data.buffer, data.length, &r, DT_NOCLIP | DT_CENTER);
            TextOut(dc, x, 50, data.buffer, data.length);
            SetBkMode(dc, OPAQUE);

            BitBlt(hdc, 0, 0, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, dc, 0, 0, SRCCOPY);
            SelectObject(dc, old_bmp);
            DeleteObject(bmp);
            DeleteDC(dc);

            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_LEFT) {
                if (LOWORD(lParam) >= cursor) {
                    cursor = 0;
                } else {
                    cursor -= LOWORD(lParam);
                }
            } else if (wParam == VK_RIGHT) {
                cursor += LOWORD(lParam);
                if (cursor > data.length) {
                    cursor = data.length;
                }
            } else if (wParam == VK_DELETE) {
                for (int i = 0; i < LOWORD(lParam); ++i) {
                    if (data.length == 0) {
                        break;
                    }
                    DynamicWStringRemove(&data, cursor, 1);
                }
                InvalidateRgn(hWnd, NULL, FALSE);
            }
            break;
        case WM_CHAR:
            if (wParam == 0x1B) {
                return 0;
            }
            _printf("C:%u %u\n", cursor, wParam);
            if (wParam == 8) {
                if (cursor != 0) {
                    --cursor;
                    DynamicWStringRemove(&data, cursor, 1);
                }
            } else if (wParam == L'\t') {
                DynamicWStringInsert(&data, cursor, L' ');
                DynamicWStringInsert(&data, cursor + 1, L' ');
                cursor += 2;
            } else {
                DynamicWStringInsert(&data, cursor, wParam);
                ++cursor;
            }

            InvalidateRgn(hWnd, NULL, FALSE);
            return 0;
        case WM_MOUSEWHEEL: {
            WORD fwKeys = GET_KEYSTATE_WPARAM(wParam);
            SHORT zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

            x += zDelta;
            InvalidateRgn(hWnd, NULL, FALSE);
            return 0;
        }
        case WM_DPICHANGED: {
            WORD dpi = HIWORD(wParam);
            RECT* new_rect = (RECT*)lParam;
           _printf("DPI changed: %d, %d %d %d %d\n", dpi, new_rect->left, new_rect->top, new_rect->right - new_rect->left, new_rect->bottom - new_rect->top);
            SetWindowPos(hWnd, NULL, new_rect->left, new_rect->top, new_rect->right - new_rect->left, new_rect->bottom - new_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int main() {
    UINT res = 0;
    HWND window = NULL;
    AttachConsole(ATTACH_PARENT_PROCESS);
    DynamicWStringCreate(&data);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HINSTANCE module = GetModuleHandle(NULL);
    WNDCLASSW wClass;
    wClass.style = 0;
    wClass.lpfnWndProc = WndProc;
    wClass.cbClsExtra = 0;
    wClass.cbWndExtra = 0;
    wClass.hInstance = module;
    wClass.hIcon = NULL;
    wClass.hCursor = LoadCursor(0, IDC_ARROW);
    wClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wClass.lpszMenuName = NULL;
    wClass.lpszClassName = L"UnicodePicker";
    ATOM id = RegisterClass(&wClass);
    if (id == 0) {
        res = GetLastError();
        goto cleanup;
    }


    window = CreateWindowExW(0, L"UnicodePicker", L"Hello world",
                             WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, module, NULL);
    if (window == NULL) {
        res = GetLastError();
        goto cleanup;
    }
    RECT dim;
    dim.left = 0;
    dim.right = 400;
    dim.top = 0;
    dim.bottom = 400;
    UINT dpi = GetDpiForWindow(window);
    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    if (!AdjustWindowRectExForDpi(&dim, style, FALSE, 0, dpi))  {
        res = GetLastError();
        goto cleanup;
    }
    int w = dim.right - dim.left;
    int h = dim.bottom - dim.top;
    _printf("DPI: %u\n", dpi);
    SetWindowPos(window, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    ShowWindow(window, SW_NORMAL);
    if (!UpdateWindow(window)) {
        res = GetLastError();
        goto cleanup;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

cleanup:
    ExitProcess(res);
}

