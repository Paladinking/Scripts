#define UNICODE
#include "cli.h"
#include "printf.h"
#include "unicode_width.h"

BOOL step_left(int *cur, int *col, CliListNode *n) {
    if (*col <= 0) {
        return FALSE;
    }
    UINT32 val = n->data.buffer[*col - 1];
    if (IS_LOW_SURROGATE(n->data.buffer[*col - 1])) {
        if (*col == 1 || !IS_HIGH_SURROGATE(n->data.buffer[*col - 2])) {
            *cur = 1;
            *col -= 1;
            return TRUE;
        }
        val = (val & 0x3FF) | ((n->data.buffer[*col - 2] & 0x3FF) << 10);
        *col -= 2;
    } else {
        *col -= 1;
    }
    *cur = UNICODE_WIDTH(val);
    return TRUE;
}

BOOL step_right(int *cur, int *col, CliListNode *n) {
    if (*col >= n->data.length) {
        return FALSE;
    }
    UINT32 val = n->data.buffer[*col];
    if (IS_HIGH_SURROGATE(n->data.buffer[*col])) {
        if (*col == n->data.length - 1 ||
            !IS_LOW_SURROGATE(n->data.buffer[*col + 1])) {
            *cur = 1;
            *col += 1;
            return TRUE;
        }
        val = ((val & 0x3FF) << 10) | (n->data.buffer[*col + 1] & 0x3FF);
        *col += 2;
        return TRUE;
    } else {
        *col += 1;
    }
    *cur = UNICODE_WIDTH(val);
    return TRUE;
}

BOOL CliListNode_setwidth(CliListNode *node, unsigned ix,
                                            SHORT width, DWORD end_ix) {
    while (node->row_capacity <= ix) {
        DWORD new_cap = node->row_capacity * 2;
        RowData *rows = HeapReAlloc(GetProcessHeap(), 0, node->row_data,
                                  new_cap * sizeof(RowData));
        if (rows == NULL) {
            return FALSE;
        }
        node->row_data = rows;
        node->row_capacity = new_cap;
    }
    node->row_data[ix].width = width;
    node->row_data[ix].end_ix = end_ix;
    return TRUE;
}

BOOL CliListNode_pushdown(CliListNode* node, int *w, int row_width, DynamicWString* pushed, int row) {
    if (row == node->rows) {
        CliListNode_setwidth(node, row, *w, node->data.length);
        ++node->rows;
    } else {
        node->row_data[row].width += *w;
    }
    int last_ix = node->row_data[row].end_ix;
    DynamicWStringClear(pushed);
    *w = 0;
    while (row_width < node->row_data[row].width) {
        int lw;
        if (step_left(&lw, &last_ix, node)) {
            node->row_data[row].width -= lw;
            *w += lw;
        } else {
            return FALSE;
        }
    }
    for (int i = last_ix; i < node->row_data[row].end_ix; ++i) {
        if (!DynamicWStringAppend(pushed, node->data.buffer[i])) {
            return FALSE;
        }
    }
    node->row_data[row].end_ix = last_ix;
    return TRUE;
}

// Insert one wchar_t into the node
// To update row lengths, also call CliListNode_pushdown with increasing row until it provides
// no more characters.
BOOL CliListNode_insert(CliListNode *node, unsigned ix, wchar_t c) {
    int row;
    if (ix == node->data.length) {
        row = node->rows - 1;
    } else {
        row = 0; 
        while (ix >= node->row_data[row].end_ix) {
            ++row;
        }
    }
    if (!DynamicWStringInsert(&node->data, ix, c)) {
        return FALSE;
    }
    for (int i = row; i < node->rows; ++i) {
        node->row_data[i].end_ix++;
    }
    return TRUE;
}

// Insert one surrogate pair into the node
BOOL CliListNode_insert_pair(CliListNode *node, unsigned ix, wchar_t c1, wchar_t c2) {
    int row;
    if (ix == node->data.length) {
        row = node->rows - 1;
    } else {
        row = 0;
        while (ix >= node->row_data[row].end_ix) {
            ++row;
        }
    }
    if (!DynamicWStringInsert(&node->data, ix, c1)) {
        return FALSE;
    }
    if (!DynamicWStringInsert(&node->data, ix + 1, c2)) {
        DynamicWStringRemove(&node->data, ix, 1);
        return FALSE;
    }
    for (int i = row; i < node->rows; ++i) {
        node->row_data[i].end_ix += 2;
    }
    return TRUE;
}

void CliListNode_free(CliListNode *node) {
    DynamicWStringFree(&node->data);
    HeapFree(GetProcessHeap(), 0, node->row_data);
}

BOOL CliListNode_create(CliListNode *node, DWORD flags, DynamicWString *data) {
    node->rows = 1;
    node->row_capacity = 4;
    node->flags = flags & ~CLI_COPY;
    if (data != NULL) {
        if (flags & CLI_COPY) {
            if (!DynamicWStringCopy(&node->data, data)) {
                return FALSE;
            }
        } else {
            node->data = *data;
        }
    } else {
        if (!DynamicWStringCreate(&node->data)) {
            return FALSE;
        }
    }
    node->row_data = HeapAlloc(GetProcessHeap(), 0, 4 * sizeof(RowData));
    if (node->row_data == NULL) {
        DynamicWStringFree(&node->data);
        return FALSE;
    }
    return TRUE;
}

void CliListNode_remove(CliListNode *node, unsigned int ix) {
    if (IS_HIGH_SURROGATE(node->data.buffer[ix])) {
        DynamicWStringRemove(&node->data, ix, 2);
    } else {
        DynamicWStringRemove(&node->data, ix, 1);
    }
}

const wchar_t *CliListNode_draw_row(CliListNode *node, CONSOLE_SCREEN_BUFFER_INFO *cInfo, DynamicWString* buf, unsigned row) {
    DynamicWStringClear(buf);
    unsigned start;
    if (row == 0) {
        start = 0;
    } else {
        start = node->row_data[row - 1].end_ix;
    }
    unsigned len = node->row_data[row].end_ix - start;
    DynamicWStringAppendCount(buf, node->data.buffer + start, len);
    return buf->buffer;
}

const wchar_t *CliListNode_draw(CliListNode *node,
                                CONSOLE_SCREEN_BUFFER_INFO *cInfo, DynamicWString* buf) {
    // If the line contains no invalid unicode and is fully visible, just print
    // it.
    if ((node->flags & (CLI_OBSCURED | CLI_INVALID_UNICODE)) == 0) {
        return node->data.buffer;
    }
    int ix = 0;
    int offset = 0;
    int width = 0;
    DynamicWStringClear(buf);
    DynamicWStringReserve(buf, node->data.length);
    while (1) {
        UINT32 val = node->data.buffer[ix];
        int w = 1;
        if (IS_HIGH_SURROGATE(node->data.buffer[ix])) {
            ++ix;
            if (IS_LOW_SURROGATE(node->data.buffer[ix])) {
                DynamicWStringAppend(buf, val);
                DynamicWStringAppend(buf, node->data.buffer[ix]);
                val = ((node->data.buffer[ix] & 0x3FF) << 10) | (val & 0x3FF);
                w = UNICODE_WIDTH(val);
                ++ix;
            } else {
                DynamicWStringAppend(buf, 0xfdff);
                w = 1;
            }
        } else {
            if (IS_LOW_SURROGATE(node->data.buffer[ix])) {
                DynamicWStringAppend(buf, 0xfdff);
                w = 1;
            } else {
                DynamicWStringAppend(buf, node->data.buffer[ix]);
                w = UNICODE_WIDTH(val);
            }
            ++ix;
        }
        width += w;
        if (width > cInfo->dwSize.X) {
            DynamicWStringInsert(buf, ix + offset - w, L'\r');
            DynamicWStringInsert(buf, ix + offset + 1 - w, L'\x1b');
            DynamicWStringInsert(buf, ix + offset + 2 - w, L'[');
            DynamicWStringInsert(buf, ix + offset + 3 - w, L'1');
            DynamicWStringInsert(buf, ix + offset + 4 - w, L'B');
            offset += 5;
            width = w;
        }
        if (ix == node->data.length) {
            return buf->buffer;
        }
    }
    return buf->buffer;
}

void validate_lines(CliList *list, int console_width) {
    list->options &= ~CLI_OBSCURED;
    for (int i = 0; i < list->element_count; ++i) {
        int width = 0;
        int cur = 0;
        list->elements[i].flags &= (~CLI_OBSCURED & ~CLI_INVALID_UNICODE);
        list->elements[i].rows = 1;
        int prev_col = 0;
        for (int col = 0; col < list->elements[i].data.length;) {
            if (width > console_width) {
                CliListNode_setwidth(&list->elements[i],
                                     list->elements[i].rows - 1, width - cur, prev_col);
                width = cur;
                list->elements[i].rows += 1;
                // Full string not visible
                list->elements[i].flags |= CLI_OBSCURED;
                list->options |= CLI_OBSCURED;
            }
            wchar_t c = list->elements[i].data.buffer[col];
            if (IS_LOW_SURROGATE(c)) {
                list->elements[i].flags |= CLI_INVALID_UNICODE;
            } else if (IS_HIGH_SURROGATE(c)) {
                wchar_t c2 = list->elements[i].data.buffer[col + 1];
                if (!IS_LOW_SURROGATE(c2)) {
                    list->elements[i].flags |= CLI_INVALID_UNICODE;
                }
            }
            prev_col = col;
            if (step_right(&cur, &col, &list->elements[i])) {
                width += cur;
            } else {
                break;
            }
        }
        CliListNode_setwidth(&list->elements[i], list->elements[i].rows - 1,
                             width, list->elements[i].data.length);
        if (width >= console_width) {
            list->elements[i].flags |= CLI_OBSCURED;
        }
    }
}

void full_draw(CliList *list, CONSOLE_SCREEN_BUFFER_INFO *cInfo) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    BOOL header = list->header != NULL;
    if (header) {
        _wprintf_h(out, L"%s\n\r", list->header);
    }
    DynamicWString b;
    DynamicWStringCreate(&b);
    validate_lines(list, cInfo->dwSize.X);
    for (int i = 0; i < list->element_count; ++i) {
        DynamicWStringClear(&b);
        DynamicWStringReserve(&b, 3 * list->elements[i].rows);
        for (int j = 0; j < list->elements[i].rows; ++j) {
            b.buffer[j] = L'\n';
            b.buffer[2 * j + list->elements[i].rows] = L'\x1b';
            b.buffer[2 * j + list->elements[i].rows + 1] = L'M';
        }
        b.buffer[3 * list->elements[i].rows] = L'\0';
        const wchar_t *s = CliListNode_draw(&list->elements[i], cInfo, &list->scratch_buffer);
        _wprintf_h(out, L"%s%s\n\r", b.buffer, s);
    }
    if (list->element_count > 0 &&
        list->elements[list->element_count - 1].rows > 1) {
        DynamicWStringClear(&b);
        DynamicWStringReserve(&b, list->elements[list->element_count - 1].rows * 2);
        int i = 0;
        for (; i < list->elements[list->element_count - 1].rows; ++i) {
            b.buffer[2 * i] = L'\x1b';
            b.buffer[2 * i + 1] = L'M';
        }
        b.buffer[2 * i] = L'\0';
        _wprintf_h(out, L"Footer%s\r", b.buffer);
    } else {
        _wprintf_h(out, L"Footer\x1bM\r");
    }
    DynamicWStringFree(&b);
}

CliList *CliList_create(const wchar_t *header, DynamicWString *elements,
                        DWORD count, DWORD flags) {
    CliList *list = HeapAlloc(GetProcessHeap(), 0, sizeof(CliList));
    if (list == NULL) {
        return list;
    }

    list->elements =
        HeapAlloc(GetProcessHeap(), 0, sizeof(CliListNode) * count);
    if (list->elements == NULL) {
        HeapFree(GetProcessHeap(), 0, list);
        return NULL;
    }
    if (!DynamicWStringCreate(&list->scratch_buffer)) {
        HeapFree(GetProcessHeap(), 0, list->elements);
        HeapFree(GetProcessHeap(), 0, list);
        return NULL;
    }

    if (flags & CLI_COPY) {
        for (DWORD i = 0; i < count; ++i) {
            if (!CliListNode_create(&list->elements[i], CLI_COPY,
                                    &elements[i])) {
                for (DWORD j = 0; j < i; ++j) {
                    CliListNode_free(&list->elements[j]);
                }
                DynamicWStringFree(&list->scratch_buffer);
                HeapFree(GetProcessHeap(), 0, list->elements);
                HeapFree(GetProcessHeap(), 0, list);
                return NULL;
            }
        }
    } else {
        for (DWORD i = 0; i < count; ++i) {
            if (!CliListNode_create(&list->elements[i], 0, &elements[i])) {
                for (DWORD j = 0; j < i; ++j) {
                    HeapFree(GetProcessHeap(), 0, list->elements[i].row_data);
                }
                DynamicWStringFree(&list->scratch_buffer);
                HeapFree(GetProcessHeap(), 0, list->elements);
                HeapFree(GetProcessHeap(), 0, list);
                return NULL;
            }
        }
    }

    list->header = header;
    list->element_count = count;
    list->elements_capacity = count;
    list->options = flags;

    return list;
}

// Make sure there is space to step down one (cursor) line
void allow_scroll(CliList *list, CONSOLE_SCREEN_BUFFER_INFO *cInfo, unsigned cursor_row, unsigned row) {
    if (cInfo->dwCursorPosition.Y + 1 < cInfo->dwSize.Y) {
        return;
    }
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (cursor_row + 1 < list->elements[row].rows) {
        // Scroll + print next line in current element
        // TODO: verify this works
        const wchar_t* r = CliListNode_draw_row(&list->elements[row], cInfo, &list->scratch_buffer, cursor_row + 1);
        _wprintf_h(out, L"\x1b[1S\x1b[1B\r%s\x1b[1A");
    } else if (row < list->element_count) {
        // Scroll + print first line in next element
        const wchar_t* r = CliListNode_draw_row(&list->elements[row + 1], cInfo, &list->scratch_buffer, 0);
        _wprintf_h(out, L"\n\r%s\x1bM", r);
    } else {
        // Scroll + print footer
        _wprintf_h(out, L"\n\r%s\x1bM", L"Footer");
    }
}

DWORD CliList_run(CliList *list) {
    DWORD res = ERROR_SUCCESS;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD old_out_mode, old_in_mode;
    if (!GetConsoleMode(out, &old_out_mode) ||
        !GetConsoleMode(in, &old_in_mode)) {
        return GetLastError();
    }

    if (!SetConsoleMode(out, ENABLE_PROCESSED_OUTPUT |
                                 ENABLE_WRAP_AT_EOL_OUTPUT |
                                 DISABLE_NEWLINE_AUTO_RETURN |
                                 ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
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
        _wprintf_h(out, L"\n\r");
    }
    if (!GetConsoleScreenBufferInfo(out, &cInfo)) {
        res = GetLastError();
        goto cleanup;
    }
    _wprintf_h(out, L"\x1b[4\x20q\x1b[22m");
    full_draw(list, &cInfo);
    DynamicWString b1, b2;
    DynamicWStringCreate(&b1);
    DynamicWStringCreate(&b2);

    int row = list->element_count - 1;
    int col = 0;
    int cursor_col = 0;
    int cursor_row = 0; // Row in current line

    BOOL insert_mode = FALSE;
    wchar_t surrogate = L'\0';
    while (1) {
        INPUT_RECORD record;
        DWORD read;
        if (!ReadConsoleInput(in, &record, 1, &read)) {
            res = GetLastError();
            break;
        }
        GetConsoleScreenBufferInfo(out, &cInfo);
        if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            validate_lines(list, cInfo.dwSize.X);
        } else if (record.EventType == KEY_EVENT) {
            BOOL ctrl_pressed = (record.Event.KeyEvent.dwControlKeyState &
                                 (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
            BOOL alt_pressed = (record.Event.KeyEvent.dwControlKeyState &
                                LEFT_ALT_PRESSED) != 0;
            WORD keycode = record.Event.KeyEvent.wVirtualKeyCode;
            if (!record.Event.KeyEvent.bKeyDown) {
                continue;
            }
            wchar_t c = record.Event.KeyEvent.uChar.UnicodeChar;
            if (keycode == VK_UP) {
                if (alt_pressed && (list->options & CLI_MOVE) != 0) {
                    if (row > 0) {
                        CliListNode temp = list->elements[row];
                        list->elements[row] = list->elements[row - 1];
                        list->elements[row - 1] = temp;
                        const wchar_t *buf1 = CliListNode_draw(&list->elements[row], &cInfo, &b1);
                        const wchar_t *buf2 = CliListNode_draw(&list->elements[row - 1], &cInfo, &b2);
                        _wprintf_h(out, L"\x1b[2K\r%s\x1bM\x1b[2K\r%s\r", buf1, buf2);
                        list->elements[row].flags |= CLI_MOVE;
                        list->elements[row - 1].flags |= CLI_MOVE;
                        row--;
                        col = 0;
                    }
                } else if (!insert_mode) {
                    if (row > 0) {
                        const wchar_t *buf = CliListNode_draw(&list->elements[row - 1], &cInfo, &b1);
                        if (list->elements[row - 1].rows == 1) {
                            _wprintf_h(out, L"\x1bM\r%s\r", buf);
                        } else {
                            DynamicWStringClear(&b2);
                            for (int i = 0; i < list->elements[row - 1].rows;
                                 ++i) {
                                DynamicWStringExtend(&b2, L"\x1bM");
                            }
                            _wprintf_h(out, L"%s\r%s\r\x1b[%dA", b2.buffer, buf,
                                       list->elements[row - 1].rows - 1);
                        }
                        row--;
                    } else {
                        _wprintf_h(out, L"\x1bM\r%s\n\r", list->header);
                    }
                }
            } else if (keycode == VK_DOWN) {
                if (alt_pressed && (list->options & CLI_MOVE) != 0) {
                    if (row < list->element_count - 1) {
                        CliListNode temp = list->elements[row];
                        list->elements[row] = list->elements[row + 1];
                        list->elements[row + 1] = temp;
                        const wchar_t *buf1 = CliListNode_draw(&list->elements[row], &cInfo, &b1);
                        const wchar_t *buf2 = CliListNode_draw(&list->elements[row + 1], &cInfo, &b2);
                        _wprintf_h(out, L"\x1b[2K\r%s\n\x1b[2K\r%s\r", buf1,
                                   buf2);
                        list->elements[row].flags |= CLI_MOVE;
                        list->elements[row + 1].flags |= CLI_MOVE;
                        row++;
                        col = 0;
                    }
                } else if (!insert_mode) {
                    if (row < list->element_count - 1) {
                        const wchar_t *buf = CliListNode_draw(
                            &list->elements[row + 1], &cInfo, &b1);
                        int i = 0;
                        int steps = list->elements[row + 1].rows +
                                    list->elements[row].rows;
                        int diff =
                            steps - (cInfo.dwSize.Y - cInfo.dwCursorPosition.Y);
                        if (diff > 0) {
                            if (list->elements[row].rows > 1) {
                                _wprintf_h(out, L"\x1b[%dB",
                                           list->elements[row].rows - 1);
                            }
                            if (diff == 1) {
                                _wprintf_h(out, L"\x1b[%dS", diff);
                            } else {
                                _wprintf_h(out, L"\x1b[%dS\x1b[%dA", diff,
                                           diff - 1);
                            }
                        } else {
                            _wprintf_h(out, L"\x1b[%dB",
                                       list->elements[row].rows);
                        }
                        if (list->elements[row + 1].rows == 1) {
                            _wprintf_h(out, L"\r%s\r", buf);
                        } else {
                            _wprintf_h(out, L"\r%s\r\x1b[%dA", buf,
                                       list->elements[row + 1].rows - 1);
                        }
                        row++;
                    } else {
                        int last = list->element_count - 1;
                        if (list->element_count == 0 ||
                            list->elements[last].rows == 1) {
                            _wprintf_h(out, L"\n\rFooter\x1bM\r");
                        } else {
                            int steps = list->elements[last].rows;
                            _wprintf_h(out, L"\x1b[%dB\n\rFooter\r\x1b[%dA",
                                       steps - 1, steps);
                        }
                    }
                }
            } else if (keycode == 0x43 && ctrl_pressed) {
                break;
            } else if (keycode == VK_ESCAPE) {
                insert_mode = FALSE;
                _wprintf_h(out, L"\x1b[4\x20q\r");
                if (cursor_row > 0) {   
                    _wprintf_h(out, L"\x1b[%dA", cursor_row);
                }
                col = 0;
                cursor_col = 0;
                cursor_row = 0;
                surrogate = L'\0';
            } else if (insert_mode) {
                int cur;
                if (keycode == VK_LEFT) {
                    if (step_left(&cur, &col, &list->elements[row])) {
                        cursor_col -= cur;
                        if (cursor_col < 0 && cursor_row > 0) {
                            --cursor_row;
                            cursor_col = list->elements[row].row_data[cursor_row].width - cur;
                            if (cInfo.dwCursorPosition.Y == 0) {
                                // Scroll up, move cursor to (0, 0) and print previous row
                                const wchar_t* r = CliListNode_draw_row(&list->elements[row], &cInfo, &b1, cursor_row);
                                _wprintf_h(out, L"\x1b[1T\x1b[1;1%s", r);
                            } else {
                                // Move to cursor_col, move up
                                _wprintf_h(out, L"\x1b[%dG\x1b[1A", cursor_col + 1);
                            }
                        } else {
                            _wprintf_h(out, L"\x1b[%dD", cur);
                        }
                    }
                } else if (keycode == VK_RIGHT) {
                    if (step_right(&cur, &col, &list->elements[row])) {
                        cursor_col += cur;
                        if (cursor_col >= cInfo.dwSize.X) {
                            allow_scroll(list, &cInfo, cursor_row, row);
                            cursor_col = 0;
                            ++cursor_row;
                            _wprintf_h(out, L"\x1b[1B\x1b[1G");
                        } else {
                            _wprintf_h(out, L"\x1b[%dC", cur);
                        }
                    }
                } else if (keycode == VK_BACK) {
                    if (step_left(&cur, &col, &list->elements[row])) {
                        CliListNode_remove(&list->elements[row], col);
                        _wprintf_h(out, L"\x1b[%dD\x1b[%dP", cur, cur);
                    }
                } else if (keycode == VK_DELETE) {
                    int cl = col;
                    if (step_right(&cur, &cl, &list->elements[row])) {
                        CliListNode_remove(&list->elements[row], col);
                        _wprintf_h(out, L"\x1b[%dP", cur);
                        list->elements[row].flags |= CLI_EDIT;
                    }
                } else if (c >= 0x20 || c == L'\t') {
                    if (IS_HIGH_SURROGATE(c)) {
                        surrogate = c;
                    } else {
                        int w;
                        if (IS_LOW_SURROGATE(c)) {
                            if (IS_HIGH_SURROGATE(surrogate)) {
                                w = UNICODE_PAIR_WIDTH(surrogate, c);
                                CliListNode_insert_pair(&list->elements[row], col, surrogate, c);
                                if (cursor_row != list->elements[row].rows) {
                                    _wprintf_h(out, L"\x1b[%d@%c%c", w, surrogate, c);
                                }                                 col += 2;
                            }
                        } else {
                            w = UNICODE_WIDTH(c);
                            CliListNode_insert(&list->elements[row], col, c);
                            if (cursor_row != list->elements[row].rows) {
                                _wprintf_h(out, L"\x1b[%d@%c", w, c);
                            }                             col++;
                        }

                        cursor_col += w;
                        _wprintf_h(out, L"%c7", L'\x1b');
                        int cur_row = cursor_row;
                        if (cur_row == list->elements[row].rows) {
                            // This is only true if cursor is at the end of the final row.
                            // In that case, we want to start pushdown at the last row that exists.
                            _wprintf_h(out, L"\x1b[1C%c7\x1b[1A", L'\x1b');
                            --cur_row;
                        }
                        do {
                            CliListNode_pushdown(&list->elements[row], &w, cInfo.dwSize.X, &list->scratch_buffer, cur_row);
                            ++cur_row;
                            if (list->scratch_buffer.length > 0) {
                                if (cur_row == list->elements[row].rows) {
                                    _wprintf_h(out, L"\n\x1b[1L\x1b[1A"); //TODO: Scroll instead of \n
                                }
                                _wprintf_h(out, L"\x1b[1B\r\x1b[%d@%s", list->scratch_buffer.length,
                                           list->scratch_buffer.buffer);
                            }
                        } while (list->scratch_buffer.length > 0);
                        _wprintf_h(out, L"%c8", L'\x1b');
                        if (cursor_col >= cInfo.dwSize.X) {
                            allow_scroll(list, &cInfo, cursor_row, row);
                            cursor_col = 0;
                            ++cursor_row;
                            _wprintf_h(out, L"%c7\x1b[1;1H()%c8\x1b[1B\x1b[1G", L'\x1b', L'\x1b');
                        }

                    }
                    list->elements[row].flags |= CLI_EDIT;
                }
            } else if (c == L'q' || c == L'Q') {
                break;
            } else if ((c == L'i' || c == L'I') &&
                       ((list->options & CLI_EDIT) != 0 ||
                        list->elements[row].flags & CLI_INSERT)) {
                insert_mode = TRUE;
                _wprintf_h(out, L"\x1b[5\x20q");
            } else if ((c == L'n' || c == L'N') &&
                       (list->options & CLI_INSERT) != 0) {
                if (c == L'n') {
                    row = row + 1;
                    _wprintf_h(out, L"\n\r\x1b[1L\x1b[5\x20q");
                } else {
                    _wprintf_h(out, L"\x1b[1L\x1b[5\x20q");
                }
                if (list->elements_capacity == list->element_count) {
                    CliListNode *el = HeapReAlloc(
                        GetProcessHeap(), 0, list->elements,
                        list->elements_capacity * 2 * sizeof(CliListNode));
                    if (el == NULL) {
                        res = ERROR_OUTOFMEMORY;
                        break;
                    }
                    list->elements_capacity = list->elements_capacity * 2;
                    list->elements = el;
                }
                memmove(list->elements + row + 1, list->elements + row,
                        (list->element_count - row) * sizeof(CliListNode));
                CliListNode_create(&list->elements[row], CLI_INSERT, NULL);
                list->element_count += 1;
                insert_mode = TRUE;
            } else if (ctrl_pressed && (keycode == 0x58) &&
                       ((list->options & CLI_DELETE) ||
                        (list->elements[row].flags & CLI_INSERT))) {
                CliListNode_free(&list->elements[row]);
                memmove(list->elements + row, list->elements + row + 1,
                        (list->element_count - row - 1) * sizeof(CliListNode));
                _wprintf_h(out, L"\x1b[1M");
                list->element_count = list->element_count - 1;
                if (row == list->element_count) {
                    --row;
                    _wprintf_h(out, L"\x1bM\r");
                }

                GetConsoleScreenBufferInfo(out, &cInfo);
                int y = cInfo.dwCursorPosition.Y;
                int h = cInfo.dwSize.Y;
                int rows = h - y;
                int elems = list->element_count - row + 1;
                if (elems > rows) {
                    int ix = list->element_count - (elems - rows);
                    _wprintf_h(out, L"\x1b[%u;0H%s\x1b[%u;0H", h,
                               list->elements[ix].data.buffer, y + 1);
                } else if (elems == rows) {
                    _wprintf_h(out, L"\x1b[%u;0H%s\x1b[%u;0H", h, L"Footer",
                               y + 1);
                }
            }
        }
    }
    for (; row < list->element_count; ++row) {
        _wprintf_h(out, L"\r%s\n\r", list->elements[row].data.buffer);
    }
    _wprintf_h(out, L"\rFooter\n\r");
    _wprintf_h(out, L"\x1b[0\x20q\x1b[22m");

cleanup:
    DynamicWStringFree(&b1);
    DynamicWStringFree(&b2);
    SetConsoleMode(out, old_out_mode);
    SetConsoleMode(in, old_in_mode);
    return res;
}

void CliList_free(CliList *list) {
    for (DWORD i = 0; i < list->element_count; ++i) {
        CliListNode_free(&list->elements[i]);
    }
    DynamicWStringFree(&list->scratch_buffer);
    HeapFree(GetProcessHeap(), 0, list);
}
