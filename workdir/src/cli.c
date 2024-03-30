#define UNICODE
#include "cli.h"
#include "printf.h"

CliList* CliList_create(const wchar_t* header, DynamicWString* elements, DWORD count, DWORD flags) {
    CliList* list = HeapAlloc(GetProcessHeap(), 0, sizeof(CliList));
    if (list == NULL) {
        return list;
    }

    list->elements = HeapAlloc(GetProcessHeap(), 0, sizeof(DynamicWString) * count);
    if (list->elements == NULL) {
        HeapFree(GetProcessHeap(), 0, list);
        return NULL;
    }
    
    if (flags & CLI_COPY) {
        for (DWORD i = 0; i < count; ++i) {
            if (!DynamicWStringCopy(&list->elements[i], &elements[i])) {
                for (DWORD j = 0; j < i; ++j) {
                    DynamicWStringFree(&list->elements[j]);
                }
                HeapFree(GetProcessHeap(), 0, list->elements);
                HeapFree(GetProcessHeap(), 0, list);
                return NULL;
            }
        }
    } else {
        memcpy(list->elements, elements, sizeof(DynamicWString) * count);
    }

    list->header = header;
    list->element_count = count;
    list->elements_capacity = count;
    list->options = flags;

    return list;
}

DWORD CliList_run(CliList* list) {
    DWORD res = ERROR_SUCCESS;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD old_out_mode, old_in_mode;
    if (!GetConsoleMode(out, &old_out_mode) || !GetConsoleMode(in, &old_in_mode)) {
        return GetLastError();
    }

    if (!SetConsoleMode(out, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN )) {
        res = GetLastError();
        goto cleanup;
    }
    if (!SetConsoleMode(in, ENABLE_WINDOW_INPUT)) {
        res = GetLastError();
        goto cleanup;
    }

    CONSOLE_SCREEN_BUFFER_INFO cInfo;
    if (!GetConsoleScreenBufferInfo(out, &cInfo)) {
        res = GetLastError();
        goto cleanup;
    }

    if (cInfo.dwCursorPosition.X > 0) {
        WriteConsole(out, L"\n\r", 2 * sizeof(wchar_t), NULL, NULL);
    }
    if (!GetConsoleScreenBufferInfo(out, &cInfo)) {
        res = GetLastError();
        goto cleanup;
    }
    BOOL header = list->header != NULL;
    if (header) {
        _wprintf_h(out, L"%d, %d: %s\n\r", old_in_mode, old_out_mode, list->header);
    }

    for (int i = 0; i < list->element_count; ++i) {
        _wprintf_h(out, L"%s\n\r", list->elements[i].buffer);
    }
    _wprintf_h(out, L"Footer\x1bM\r");
    _wprintf_h(out, L"\x1b[4\x20q");
    int x = list->element_count - 1;
    int y = 0;

    BOOL insert_mode = FALSE;
    while (1) {
        INPUT_RECORD record;
        DWORD read;
        if (!ReadConsoleInput(in, &record, 1, &read)) {
            res = GetLastError();
            break;
        }
        if (record.EventType == KEY_EVENT) {
            BOOL ctrl_pressed = (record.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
            BOOL alt_pressed = (record.Event.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED) != 0;
            WORD keycode = record.Event.KeyEvent.wVirtualKeyCode;
            if (!record.Event.KeyEvent.bKeyDown) {
                continue;
            }
            wchar_t c = record.Event.KeyEvent.uChar.UnicodeChar;
            if (keycode == VK_UP) {
                if (alt_pressed && (list->options & CLI_MOVE) != 0) {
                    if (x > 0) {
                        DynamicWString temp = list->elements[x];
                        list->elements[x] = list->elements[x - 1];
                        list->elements[x - 1] = temp;
                        _wprintf_h(out, L"\x1b[2K\r%s\x1bM\x1b[2K\r%s\r",
                            list->elements[x].buffer, list->elements[x - 1].buffer);
                        x--;
                        y = 0;
                    }
                } else if (!insert_mode) {
                    if (x > 0) {
                        _wprintf_h(out, L"\x1bM\r%s\r", list->elements[x - 1].buffer);
                        x--;
                    } else {
                        _wprintf_h(out, L"\x1bM\r%s\n\r", list->header);
                    }
                }
            } else if (keycode == VK_DOWN) {
                if (alt_pressed && (list->options & CLI_MOVE) != 0) {
                    if (x < list->element_count - 1) {
                        DynamicWString temp = list->elements[x];
                        list->elements[x] = list->elements[x + 1];
                        list->elements[x + 1] = temp;
                        _wprintf_h(out, L"\x1b[2K\r%s\n\x1b[2K\r%s\r",
                            list->elements[x].buffer, list->elements[x + 1].buffer);
                        x++;
                        y = 0;
                    }
                } else if (!insert_mode) {
                    if (x < list->element_count - 1) {
                        _wprintf_h(out, L"\n\r%s\r", list->elements[x + 1].buffer);
                        x++;
                    } else {
                        _wprintf_h(out, L"\n\rFooter\x1bM\r");
                    }
                }
            } else if ((c == L'c' || c == L'C') && ctrl_pressed) {
                break;
            } else if (keycode == VK_ESCAPE) {
                insert_mode = FALSE;
                _wprintf_h(out, L"\x1b[4\x20q\r");
                y = 0;
            } else if (insert_mode) {
                if (keycode == VK_LEFT) {
                    if (y > 0) {
                        _wprintf_h(out, L"\x1b[1D");
                        y--;
                    }
                } else if (keycode == VK_RIGHT) {
                    if (y < list->elements[x].length) {
                        _wprintf_h(out, L"\x1b[1C");
                        y++;
                    }
                } else if (keycode == VK_BACK) {
                    if (y > 0) {
                       DynamicWStringRemove(&list->elements[x], y - 1, 1);
                       y--;
                       _wprintf_h(out, L"\x1b[1D\x1b[1P");
                    }
                } else if (keycode == VK_DELETE) {
                    if (y < list->elements[x].length) {
                        DynamicWStringRemove(&list->elements[x], y, 1);
                        _wprintf_h(out, L"\x1b[1P");
                    }
                } 
                if (c >= 0x20 || c == L'\t') {
                    if (c == L'\t' && (list->options & CLI_KEEP_TABS) == 0) {
                        DynamicWStringInsert(&list->elements[x], y, L' ');
                        DynamicWStringInsert(&list->elements[x], y + 1, L' ');
                        DynamicWStringInsert(&list->elements[x], y + 2, L' ');
                        DynamicWStringInsert(&list->elements[x], y + 3, L' ');
                        _wprintf_h(out, L"\x1b[4@    ");
                        y += 4;
                    } else {
                        if (IS_LOW_SURROGATE(c)) {
                            DynamicWStringInsert(&list->elements[x], y, L'X');
                            _wprintf_h(out, L"\x1b[1@X");
                        } else if (IS_HIGH_SURROGATE(c)) {
                            DynamicWStringInsert(&list->elements[x], y, L'Y');
                            _wprintf_h(out, L"\x1b[1@X");
                        } else {
                            DynamicWStringInsert(&list->elements[x], y, c);
                            _wprintf_h(out, L"\x1b[1@%c", c);
                        }
                        y++;
                    }    
                }
            } else if (c == L'q' || c == L'Q') {
                break;
            } else if ((c == L'i' || c == L'I') && (list->options & CLI_EDIT) != 0) {
                insert_mode = TRUE;
                _wprintf_h(out, L"\x1b[5\x20q");
            } else if ((c == L'n' || c == L'N') && (list->options & CLI_INSERT) != 0) {
                if (c == L'n') {
                    x = x + 1;
                    _wprintf_h(out, L"\n\r\x1b[1L\x1b[5\x20q");
                } else {
                    _wprintf_h(out, L"\x1b[1L\x1b[5\x20q");
                }
                if (list->elements_capacity == list->element_count) {
                    DynamicWString* el = HeapReAlloc(GetProcessHeap(), 0, list->elements,
                            list->element_count * 2 * sizeof(DynamicWString));
                    if (el == NULL) {
                        res = ERROR_OUTOFMEMORY;
                        break;
                    }
                    list->elements_capacity = list->elements_capacity * 2;
                    list->elements = el;
                }
                memmove(list->elements + x + 1, list->elements + x, (list->element_count - x) * sizeof(DynamicWString));
                DynamicWStringCreate(&list->elements[x]);
                list->element_count += 1;
                insert_mode = TRUE;
            }
        }
    }
    for (;x < list->element_count; ++x) {
        _wprintf_h(out, L"\r%s\n\r", list->elements[x].buffer);
    }
    WriteConsole(out, L"\r", sizeof(wchar_t), NULL, NULL);
    _wprintf_h(out, L"\x1b[0\x20q");
    
cleanup:
    SetConsoleMode(out, old_out_mode);
    SetConsoleMode(in, old_in_mode);
    return res;
}

void CliList_free(CliList *list) {
    for (DWORD i = 0; i < list->element_count; ++i) {
        DynamicWStringFree(&list->elements[i]);
    }
    HeapFree(GetProcessHeap(), 0, list);
}

