@echo off & python -x %~f0 %* & exit /B
import math
import sys
import collections
import builtins
import socket
import struct
from math import sin, cos, pi, sqrt, log, factorial

old_bin = bin
old_hex = hex
old_log = log

def use(lib):
    imported = builtins.__import__(lib)
    globals()[lib] = imported
    return imported

def libdir(lib):
    return dir(use(lib))

def ln(x):
    return old_log(x)

def log(x):
    return old_log(x, 10)

def logn(x, n):
    return old_log(x, n)

def bin(v, n = None):
    val = old_bin(v)[2:]
    if n is None:
        return val
    return "0" * (n - len(val)) + val 

def binf(v, n = None):
    return "0b" + bin(v, n)

def hex(v, n = None):
    val = old_hex(v)[2:]
    if n is None:
        return val
    return "0" * (n - len(val)) + val

def hexf(v, n = None):
    return "0x" + hex(v, n)

def mapping(f):
	return lambda x: map(f, x)

def reduce(f, first, it):
    for val in it:
        first = f(first, val)
    return first

def consume(it):
    collections.deque(maxlen=0).extend(it)
    return Void

class ForEvery:
    def __init__(self):
        self.calls = []
    def __add__(self, other):
        self.calls.append(other)
        return self
    def __call__(self, it):
        mapped = it
        for call in self.calls:
            mapped = map(call, mapped)
        return mapped

forevery = ForEvery()

def forall(f):
	return lambda it: foreach(f, it)

def foreach(f, it):
    for val in it:
        f(val)

fold = reduce

def reduce_through(f, val, it):
    for v in it:
        f(val, v)
    return val

fold_through = reduce_through

def index(i):
    return lambda x : x[i]

def callb(f, *args):
    return lambda x: f(*args, x)

def call(f, *args, pos = 0):
    first = args[0:pos]
    second = args[pos:]
    return lambda x: f(*first, x, *second)

old_int = int

def int(s, base = None):
    if not isinstance(s, str):
        return old_int(s)
    if not base is None:
        return old_int(s, base)
    if len(s) > 2 and s[0] == '0':
        if s[1] == 'x' or s[1] == 'X':
            return old_int(s, 16)
        elif s[1] == 'b' or s[1] == 'B':
            return old_int(s, 2)
        elif s[1] == 'o' or s[1] == 'O':
            return old_int(s, 8)
    return old_int(s, 10)

int.from_bytes = old_int.from_bytes
int.to_bytes = old_int.to_bytes
int.as_integer_ratio = old_int.as_integer_ratio
try:
	int.bit_count = old_int.bit_count
except:
	pass
int.bit_length = old_int.bit_length

def average(lst):
    return sum(lst) / len(lst)

def factors(n):
        step = 2 if n%2 else 1
        return set(reduce(list.__add__, [],
                    ([i, n//i] for i in range(1, int(sqrt(n))+1, step) if n % i == 0)))

variables = dict()

def store(name):
    def f(x):
        variables[name] = x
    return f

def get(name):
    return variables[name]

def do(*data, ret=0):
    return data[ret]

def gdb_wide(s):
    out = ''
    i = 0
    while i < len(s):
        if s[i] == '\\':
            out += chr(int(s[i + 1:i + 4], base=8))
            i += 4
        else:
            raise ValueError('Invalid gdb wide string')
    return out

def c_array(data):
    if hasattr(data, "read"):
        data = data.read()
        return "{" + str([c for c in data])[1:-1] + "}"
    return "{" + str([c for c in data])[1:-1] + "}"


def send_file(con, name, data=None):
    if isinstance(con, tuple):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(3)
        s.connect(con)
        send_file(s, name, data)
        s.close()
        return
    if isinstance(name, str):
        b_name = name.encode('utf-8')
    else:
        b_name = name
        name = name_b.decode('utf-8')
    if isinstance(data, str):
        data = data.encode('utf-8')
    elif data is None:
        with open(name, 'rb') as file:
            data = file.read()
    b_name_size = struct.pack('<Q', len(name))
    b_data_size = struct.pack('<Q', len(data))
    con.sendall(b_name_size)
    con.sendall(b_name)
    con.sendall(b_data_size)
    con.sendall(data)

def get_file(con, write=True):
    if isinstance(con, tuple):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(con)
        s.listen(1)
        c, addr = s.accept()
        res = get_file(c, write)
        c.close()
        s.close()
        return res
    b_name_size = con.recv(8)
    name_size = struct.unpack('<Q', b_name_size)[0]
    name = b''
    while len(name) < name_size:
        name += con.recv(min(4096, name_size - len(name)))
    b_data_size = con.recv(8)
    data_size = struct.unpack('<Q', b_data_size)[0]
    data = b''
    while len(data) < data_size:
        data += con.recv(min(4096, data_size - len(data)))
    if write:
        with open(name.decode('utf-8', errors='ignore'), 'wb') as file:
            file.write(data)

class Void:
    def __init__(*args, **kwars):
        pass

def main():
    if len(sys.argv) < 2:
        return
    arg = ""
    for val in sys.argv[1:]:
        arg += val + " "
    args = arg.split("!")
    first = args[0].split(";")
    res = eval(first[0])
    for arg in first[1:]:
        _ = res
        res = eval(arg)

    for step in args[1:]:
        parts = step.split(";")
        _ = res
        res = eval(parts[0])(res)
        for part in parts[1:]:
            _ = res
            res = eval(part)
    if not res is Void and not isinstance(res, Void):
        print(res)

if __name__ == "__main__":
    main()
