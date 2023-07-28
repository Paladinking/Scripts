@echo off & python -x %~f0 %* & exit /B
import math
import sys
from math import sin, cos, pi, sqrt, log

old_bin = bin
old_hex = hex
old_log = log

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

def reduce(f, first, it):
    for val in it:
        first = f(first, val)
    return first
    
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

def call(f, *args, pos = 0):
    first = args[0:pos]
    second = args[pos:]
    return lambda x : f(*first, x, *second)

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
int.bit_count = old_int.bit_count
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
    print(res)

if __name__ == "__main__":
    main()
