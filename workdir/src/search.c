#define UNICODE
#include <windows.h>
#include "args.h"
#include "printf.h"
#include <strsafe.h>


#define MAX_CLIP 4096

wchar_t* read_clipboard() {
    if (!OpenClipboard(NULL)) {
        return NULL;
    }
    wchar_t* res = NULL;
    static wchar_t clip_buf[MAX_CLIP];
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data) {
        wchar_t* clip_data = GlobalLock(data);
        if (clip_data) {
            size_t size = GlobalSize(clip_data);
            if (size > MAX_CLIP * sizeof(wchar_t)) {
                size = MAX_CLIP * sizeof(wchar_t);
            }
            size_t len;
            if (SUCCEEDED(StringCbLengthW(clip_data, MAX_CLIP, &len))) {
                memcpy(clip_buf, clip_data, len);
                res = clip_buf;
            }
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return res; 
}

#define ANY_CTRL_PRESSED (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)

int main() {
    int argc;
    LPWSTR* argv = parse_command_line(GetCommandLine(), &argc);
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD console_mode_in, console_mode_out;
    if (!GetConsoleMode(out, &console_mode_out) || !GetConsoleMode(in, &console_mode_in)) {
        return 1;
    }
    SetConsoleMode(out, console_mode_out | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleMode(in, console_mode_in & ~ENABLE_PROCESSED_INPUT);

    BOOL running = TRUE;
    while (running) {
        INPUT_RECORD records[5];
        DWORD read = 0;
        if (!ReadConsoleInput(in, records, 5, &read)) {
            return 1;
        }
        for (int i = 0; i < read; ++i) {
            if (records[i].EventType != KEY_EVENT || !records[i].Event.KeyEvent.bKeyDown) {
                continue;
            }
    
            DWORD state = records[i].Event.KeyEvent.dwControlKeyState;
            WORD key_code = records[i].Event.KeyEvent.wVirtualKeyCode;
            if (key_code == VK_ESCAPE || (key_code == 0x43 && (state & ANY_CTRL_PRESSED))) {
                running = FALSE;
                break;
            }
            if (key_code == 0x56 && (state & ANY_CTRL_PRESSED)) {
                wchar_t* clip_data = read_clipboard();
                if (clip_data) {
                    _printf_h(out, "%S", clip_data);
                }
            }
            if (state & ANY_CTRL_PRESSED) {
                continue;
            }
            if (key_code == VK_RETURN) {
                _printf_h(out, "\n");
            }
            wchar_t c = records[i].Event.KeyEvent.uChar.UnicodeChar;
            if (key_code == VK_BACK) {
                _printf_h(out, "%lc %lc", 8, 8);
            } else {
                _printf_h(out, "%lc", records[i].Event.KeyEvent.uChar.UnicodeChar);
            }
        }

    }
    SetConsoleMode(out, console_mode_out);
    SetConsoleMode(in, console_mode_in);

    return 0;
}
