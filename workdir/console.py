import sys
import os
import cmd
from typing import IO, List, Tuple

if sys.platform == 'win32':
    import ctypes
    from ctypes import wintypes
    import msvcrt

    class COORD(ctypes.Structure):
        _fields_ = [("X", wintypes.SHORT),
                    ("Y", wintypes.SHORT)]

    class SMALL_RECT(ctypes.Structure):
        _fields_ = [("Left", wintypes.SHORT),
                    ("Top", wintypes.SHORT),
                    ("Right", wintypes.SHORT),
                    ("Bottom", wintypes.SHORT)]

    class CONSOLE_SCREEN_BUFFER_INFO(ctypes.Structure):
        _fields_ = [
            ("dwSize", COORD),
            ("dwCursorPosition", COORD),
            ("wAttributes", wintypes.WORD),
            ("srWindow", SMALL_RECT),
            ("dwMaximumWindowSize", COORD)
        ]


    GENERIC_WRITE = 0x40000000
    GENERIC_READ = 0x80000000
    CONSOLE_TEXTMODE_BUFFER = 1

    get_screen_buffer_info = ctypes.windll.kernel32.GetConsoleScreenBufferInfo
    get_screen_buffer_info.argtypes = [
        wintypes.HANDLE, ctypes.POINTER(CONSOLE_SCREEN_BUFFER_INFO)
    ]
    get_screen_buffer_info.restype = wintypes.BOOL

    create_buffer = ctypes.windll.kernel32.CreateConsoleScreenBuffer
    create_buffer.argtypes = [
        wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
        wintypes.DWORD, ctypes.c_void_p
    ]
    create_buffer.restype = wintypes.HANDLE

    set_buffer = ctypes.windll.kernel32.SetConsoleActiveScreenBuffer
    set_buffer.argtypes = [wintypes.HANDLE]
    set_buffer.restype = wintypes.BOOL

    set_console_mode = ctypes.windll.kernel32.SetConsoleMode
    set_console_mode.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    set_console_mode.restype = wintypes.BOOL

    get_console_mode = ctypes.windll.kernel32.GetConsoleMode
    get_console_mode.argtypes = [wintypes.HANDLE, wintypes.LPDWORD]
    get_console_mode.restype = wintypes.BOOL

    get_std_handle = ctypes.windll.kernel32.GetStdHandle
    get_std_handle.argtypes = [wintypes.DWORD]
    get_std_handle.restype = wintypes.HANDLE

    write_console = ctypes.windll.kernel32.WriteConsoleW
    write_console.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                              wintypes.DWORD, wintypes.LPDWORD, 
                              wintypes.LPVOID]
    write_console.restype = wintypes.BOOL

    close_handle = ctypes.windll.kernel32.CloseHandle
    close_handle.argtypes = [wintypes.HANDLE]
    close_handle.restype = wintypes.BOOL
        
    ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x4


    class SubWindow(IO[str]):
        def __init__(self, x: int, y: int, w: int, h: int,
                     term: 'Win32Terminal', restore_pos: bool=False) -> None:
            self._term = term
            self._x = x
            self._y = y
            self._w = w
            self._h = h

            self._history: List[str] = []

            self._lines = ['']

            self._restore_pos = restore_pos

        def readline(self, limit: int = -1, /) -> str:
            line_count = len(self._lines)
            line_len = len(self._lines[-1])

            history_ix = len(self._history)

            pos = 0

            line = ''
            while True:
                s = msvcrt.getch()
                if s == b'\r':
                    s = b'\n'

                if s == b'\n':
                    pos = len(line)

                if s == b'\x1A':
                    return ''
                if s == b'\xe0':
                    s = msvcrt.getch()
                    if s == b'\x4d': # Arror right
                        if pos >= len(line):
                            continue
                        pos += 1
                    elif s == b'\x4b': # Arror left
                        if pos <= 0:
                            continue
                        pos -= 1
                    elif s == b'\x48': # Arrow up
                        if history_ix == 0:
                            continue
                        history_ix -= 1
                        line = self._history[history_ix]
                        pos = len(line)
                    elif s == b'\x50': # Arrow down
                        if history_ix + 1 >= len(self._history):
                            continue
                        history_ix += 1
                        line = self._history[history_ix]
                        pos = len(line)
                    else:
                        self.write("'" + hex(s[0]) + "'")
                        continue
                elif s == b'\x08':
                    if pos > 0:
                        pos -= 1
                        line = line[:pos] + line[pos + 1:]
                        if len(self._lines) != line_len:
                            self.clear()
                else:
                    s = s.decode(errors="replace")
                    line = line[:pos] + s + line[pos:]
                    pos += 1

                self._lines = self._lines[:line_count]
                self._lines[line_count - 1] = self._lines[line_count - 1][:line_len]
                self.write(line[:pos])

                self._restore_pos = True
                self.write(line[pos:])
                self._restore_pos = False

                if s == '\x03':
                    return ''

                if s == '\n':
                    break

            if len(line.strip()) > 0 and (len(self._history) == 0 or line != self._history[-1]):
                self._history.append(line[:-1])

            if limit >= 0:
                line = line[:limit]

            return line

        def clear(self) -> None:
            s = ""
            for i in range(self._h):
                to_pos = f"\x1B[{self._y + i};{self._x}H"
                s += to_pos + " " * self._w + to_pos
            x, y = self._term.get_pos()
            if x >= 0 and y >= 0:
                s += f"\x1B[{y};{x}H"
            self._term.write(s)

        def write(self, s: str) -> int:
            lines = s.split('\n')
            ix = 0

            lines[0] = self._lines[-1] + lines[0]
            self._lines.pop()

            while ix < len(lines):
                wlen = len(lines[ix])
                if wlen > self._w:
                    p1 = lines[ix][:self._w]
                    p2 = lines[ix][self._w:]
                    lines[ix] = p2
                    lines.insert(ix, p1)
                ix += 1
            
            self._lines.extend(lines)

            if len(self._lines) > self._h:
                self._lines = self._lines[len(self._lines) - self._h:]

            s = f"\x1B[{self._y};{self._x}H" + " " * self._w + f"\x1B[{self._y};{self._x}H"
            for ix, line in enumerate(self._lines[:-1]):
                to_pos = f"\x1B[{self._y + ix + 1};{self._x}H"
                s += line + to_pos + " " * self._w + to_pos
            s += lines[-1]

            if self._restore_pos:
                x, y = self._term.get_pos()
                if x >= 0 and y >= 0:
                    s += f"\x1B[{y};{x}H"

            self._term.write(s)

            return len(s)


    class Win32Terminal(IO[str]):
        def __init__(self) -> None:
            self._default_out: wintypes.HANDLE = get_std_handle(-11)

            mode = wintypes.DWORD()
            if not get_console_mode(self._default_out, ctypes.byref(mode)):
                raise RuntimeError("Failed to get console mode")

            mode = wintypes.DWORD(mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
            
            set_console_mode(self._default_out, mode)

            self._active_out = self._default_out

        def enter_alternate_mode(self) -> None:
            if self._active_out != self._default_out:
                return

            buf = create_buffer(GENERIC_WRITE | GENERIC_READ, 3, 0, 
                                CONSOLE_TEXTMODE_BUFFER, 0)
            set_buffer(buf)

            self._active_out = buf

        def leave_alternate_mode(self) -> None:
            if self._active_out == self._default_out:
                return
            set_buffer(self._default_out)
            close_handle(self._active_out)
            self._active_out = self._default_out

        def write(self, s: str) -> int:
            written = wintypes.DWORD(0)
            write_console(self._active_out, s, len(s), ctypes.byref(written), 0)

            return written.value

        def writeln(self, s: str) -> None:
            self.write(s + '\n')

        def get_size(self) -> Tuple[int, int]:
            info = CONSOLE_SCREEN_BUFFER_INFO()
            if not get_screen_buffer_info(self._active_out, ctypes.byref(info)):
                return (-1, -1)
            width = info.srWindow.Right - info.srWindow.Left + 1
            height = info.srWindow.Bottom - info.srWindow.Top + 1
            return (width, height)

        def get_pos(self) -> Tuple[int, int]:
            info = CONSOLE_SCREEN_BUFFER_INFO()
            if not get_screen_buffer_info(self._active_out, ctypes.byref(info)):
                return (-1, -1)
            return (info.dwCursorPosition.X + 1, info.dwCursorPosition.Y + 1)


    Terminal = Win32Terminal



class SspCmd(cmd.Cmd):
    intro = "SSP terminal"
    prompt = '(ssp) '

    def __init__(self, term: Terminal) -> None:
        self._term = term
        sw = SubWindow(12, 30, 40, 20, term)
        super().__init__(stdout=sw, stdin=sw)

    def do_EOF(self, arg) -> bool:
        return True

    def do_nice(self, arg: str) -> None:
        self._term.writeln("b\x1B[1;1H" + arg)

def main() -> None:
    term = Terminal()
    term.enter_alternate_mode()

    sw1 = SubWindow(10, 10, 40, 20, term, restore_pos=True)

    w, h = term.get_size()
    term.writeln(f"{w}, {h}")

    sw1.write("Hello\nWorld\n")

    import time

    #for i in range(30):
    #    sw1.write(f"This is a test {i}\n")
    #    if i == 15:
    #        sw1.write(f"This is a very very very very very very long test {i}\n")
    #    time.sleep(0.4)

    ssp_cmd = SspCmd(term)
    ssp_cmd.use_rawinput = False

    ssp_cmd.cmdloop()
    term.write("Done")

    term.leave_alternate_mode()
    term.writeln("This is a test")


if __name__ == "__main__":
    main()
