@echo off & python -x %~f0 %* & exit /B
import math
import sys
import collections
import builtins
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
	return lambda x: foreach(f, x)

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
