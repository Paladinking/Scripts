import pathlib
import subprocess
import os.path
import time
import sys
import shutil

from typing import List, Optional, Union

BUILD_DIR: str
BIN_DIR: str

DIRECTORIES = dict()
OBJECTS = dict()
EXECUTABLES = dict()
CUSTOMS = dict()

CLFLAGS: str
LINKFLAGS: str

WORKDIR: pathlib.Path

class Obj:
    def __init__(self, name: str, source: str, cmp_flags="", namespace="") -> None:
        self.name = name
        self.namespace = namespace
        self.source = source
        self.cmp_flags=cmp_flags

class Cmd:
    def __init__(self, name: str, cmd: str, depends: list, directory: str) -> None:
        self.depends = []
        for dep in depends:
            if isinstance(dep, str):
                self.depends.append(dep)
            else:
                assert hasattr(dep, "name")
                self.depends.append(getattr(dep, "name"))
        self.name = name
        self.cmd = cmd
        self.dir = directory

class Exe:
    def __init__(self, name: str, objs: List[Obj], cmds: List[Cmd], link_flags="", dll: bool=False):
        self.name = name
        self.objs = objs
        self.cmds = cmds
        self.link_flags = link_flags
        self.dll = dll

class Mingw:
    def compile_obj(self, obj: Obj) -> str:
        return f"gcc.exe -c -o {BUILD_DIR}\\{obj.name} {obj.cmp_flags} {obj.source}"

    def find_headers(self, obj: Obj) -> List[str]:
        cmd = f"gcc.exe -MM {obj.cmp_flags} {obj.source}"
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            print(res)
            raise RuntimeError("Failed generating headers")
        line = res.stdout.decode().replace("\\\r\n", "")
        return line.split()[2:]

    def link_exe(self, exe: Exe) -> str:
        row = " ".join([f"{BUILD_DIR}\\{obj.name}" for obj in exe.objs] + [f"{cmd.dir}\\{cmd.name}" for cmd in exe.cmds])
        return f"gcc.exe -o {BIN_DIR}\\{exe.name} {exe.link_flags} {row}"

    def link_dll(self, exe: Exe) -> str:
        row = " ".join([f"{BUILD_DIR}\\{obj.name}" for obj in exe.objs] + [f"{cmd.dir}\\{cmd.name}" for cmd in exe.cmds])
        return f"gcc.exe -shared -o {BIN_DIR}\\{exe.name} {exe.link_flags} {row}"

    def define(self, key: str, val: Optional[str]) -> str:
        if val:
            return f"-D{key}={val}"
        return f"-D{key}"



class Msvc:
    def compile_obj(self, obj: Obj) -> str:
        return f"cl.exe /c /Fo:{BUILD_DIR}\\{obj.name} {obj.cmp_flags} {obj.source}"

    def find_headers(self, obj: Obj) -> List[str]:
        cl = shutil.which("cl.exe")
        if cl is None:
            raise RuntimeError("Failed to find cl.exe executable")
        cmd = f"{cl} /P /showIncludes /FiNUL {obj.cmp_flags} {obj.source}"
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            print(res, file=sys.stderr)
            raise RuntimeError("Failed generating headers")
        headers = dict()
        for line in res.stderr.decode().replace("\r\n", "\n").split("\n"):
            if not line.startswith("Note: including file:"):
                continue
            path = pathlib.Path(line[21:].strip()).resolve()
            try:
                path = path.relative_to(WORKDIR)
                headers[str(path)] = None
            except ValueError:
                continue
        return list(headers.keys())
    
    def link_exe(self, exe: Exe) -> str:
        row = " ".join([f"{BUILD_DIR}\\{obj.name}" for obj in exe.objs] + [f"{cmd.dir}\\{cmd.name}" for cmd in exe.cmds])
        return f"link /OUT:{BIN_DIR}\\{exe.name} {exe.link_flags} {row}"

    def link_dll(self, exe: Exe) -> str:
        row = " ".join([f"{BUILD_DIR}\\{obj.name}" for obj in exe.objs] + [f"{cmd.dir}\\{cmd.name}" for cmd in exe.cmds])
        return f"link /OUT:{BIN_DIR}\\{exe.name} /DLL {exe.link_flags} {row}"

    def define(self, key: str, val: Optional[str]) -> str:
        if val:
            return f"/D{key}={val}"
        return f"/D{key}"


def Object(name: str, source: str, cmp_flags=None, namespace="") -> Obj:
    if cmp_flags is None:
        cmp_flags = CLFLAGS
    source = str(pathlib.PurePath(source))
    if namespace:
        name = namespace + "\\" + name
    obj = Obj(name, source, cmp_flags, namespace)

    if namespace:
        DIRECTORIES[namespace] = None

    if name in OBJECTS:
        other = OBJECTS[name]
        if other.cmp_flags == obj.cmp_flags and other.source == obj.source:
            return other
        raise RuntimeError(f"Duplicate object {name}")
    OBJECTS[name] = obj
    return obj

def Executable(name: str, *sources, cmp_flags=None, link_flags=None, namespace="", dll: bool=False) -> Exe:
    if cmp_flags is None:
        cmp_flags = CLFLAGS
    if link_flags is None:
        link_flags = LINKFLAGS
    objs = []
    cmds = []
    for src in sources:
        if isinstance(src, str):
            obj_name = str(pathlib.PurePath(src).with_suffix('.obj').name)
            obj = Object(obj_name, src, cmp_flags, namespace)
        else:
            if isinstance(src, Cmd):
                if src not in cmds:
                    cmds.append(src)
                continue
            assert isinstance(src, Obj)
            obj = src
        if obj not in objs:
            objs.append(obj)
    exe = Exe(name, objs, cmds, link_flags, dll)
    if name in EXECUTABLES:
        raise RuntimeError(f"Duplicate exe {name}")
    EXECUTABLES[name] = exe
    return exe

def Command(name: str, cmd: str, *depends, directory=None) -> Cmd:
    if directory is None:
        directory = BUILD_DIR
    if name in CUSTOMS:
        raise RuntimeError(f"Duplicate command {name}")
    DIRECTORIES[directory] = None
    command = Cmd(name, cmd, list(depends), directory)
    CUSTOMS[name] = command
    return command

def CopyToBin(*src: str) -> List[Cmd]:
    res = []
    for file in src:
        f = pathlib.PurePath(file)
        name = pathlib.PurePath(file).name
        res.append(Command(name, f"type {f} > {BIN_DIR}\\{name}", str(f), directory=BIN_DIR))
    return res

BACKEND: Union[Msvc, Mingw]

def define(key: str, val: Optional[str]=None) -> str:
    return BACKEND.define(key, val)

def find_headers(obj: Obj) -> list[str]:
    import json
    data = None
    try:
        with open('comp_cache.json', 'r') as file:
            data = json.load(file)
            cache = data[obj.name]
            assert 'stamp' in cache and isinstance(cache['stamp'], float)
            assert 'headers' in cache and isinstance(cache['headers'], list)
        t = os.path.getmtime(obj.source)
        for header in cache['headers']:
            assert isinstance(header, str)
            stamp = os.path.getmtime(header)
            if stamp > t:
                t = stamp
        if t < cache['stamp']:
            return cache['headers']
    except Exception:
        cache = None
    headers = BACKEND.find_headers(obj)
    if data is None:
        data = dict()

    data[obj.name] = {'stamp': time.time(), 'headers': headers}
    try:
        with open('comp_cache.json', 'w') as file:
            json.dump(data, file, indent=4)
    except Exception:
        pass
    return headers
        
def makefile():
    print("all:", end="")
    for obj in OBJECTS.values():
        p = pathlib.Path(obj.source).resolve()
        if not p.is_file():
            print(f"File '{p}' does not exist", file=sys.stderr)
            return

    for key, exe in EXECUTABLES.items():
        print(" \\")
        print(f"\t{BIN_DIR}\\{exe.name}", end="")
    for key, cmd in CUSTOMS.items():
        if cmd.dir != BIN_DIR:
            continue
        print(" \\")
        print(f"\t{BIN_DIR}\\{cmd.name}", end="")
    print("\n")

    for d in DIRECTORIES.keys():
        print(f"{d}:\n\t-@ if NOT EXIST \"{d}\" mkdir \"{d}\"")
    print()

    for key, obj in OBJECTS.items():
        if obj.namespace:
            bld_dir = f"{BUILD_DIR}\\{obj.namespace}"
        else:
            bld_dir = BUILD_DIR
        headers = find_headers(obj)
        depends = " ".join([obj.source] + headers + [bld_dir])
        print(f"{BUILD_DIR}\\{obj.name}: {depends}")
        print(f"\t{BACKEND.compile_obj(obj)}")
    print()
    for key, exe in EXECUTABLES.items():
        row = " ".join([f"{BUILD_DIR}\\{obj.name}" for obj in exe.objs] + [f"{cmd.dir}\\{cmd.name}" for cmd in exe.cmds])
        print(f"{BIN_DIR}\\{exe.name}: {row} {BIN_DIR}")
        if exe.dll:
            print(f"\t{BACKEND.link_dll(exe)}")
        else:
            print(f"\t{BACKEND.link_exe(exe)}")
    print()
    for key, cmd in CUSTOMS.items():
        row = " ".join(cmd.depends + [cmd.dir])
        print(f"{cmd.dir}\\{cmd.name}: {row}")
        print(f"\t{cmd.cmd}")
    print()
    print(f"clean:\n\tdel /S /Q {BUILD_DIR}\\*\n\tdel /S /Q {BIN_DIR}\\*")

def compile_commands():
    directory = str(WORKDIR.resolve())
    res = []
    for obj in OBJECTS.values():
        json = dict()
        json["directory"] = directory
        json["command"] = BACKEND.compile_obj(obj)
        json["file"] = str(pathlib.Path(obj.source).resolve())
        res.append(json)
    import json
    print(json.dumps(res, indent=4))

all_backends = {"msvc": Msvc(), "mingw": Mingw()}
active_backends = {}

def add_backend(name: str, buildir: str, bindir: str, workdir: pathlib.Path, clflags: str, linkflags: str):
    if name.lower() not in all_backends:
        raise RuntimeError("Invalid backend")
    backend = all_backends[name.lower()]
    backend.buildir = buildir
    backend.bindir = bindir
    backend.workdir = workdir
    backend.clflags = clflags
    backend.linkflags = linkflags
    active_backends[name.lower()] = backend



def set_backend(name: str) -> None:
    global BUILD_DIR, BIN_DIR, WORKDIR, CLFLAGS, LINKFLAGS, BACKEND
    if name.lower() not in active_backends:
        raise RuntimeError("Invalid backend")
    backend = active_backends[name.lower()]
    BUILD_DIR = backend.buildir
    BIN_DIR = backend.bindir
    WORKDIR = backend.workdir
    CLFLAGS = backend.clflags
    LINKFLAGS = backend.linkflags
    DIRECTORIES[BUILD_DIR] = None
    DIRECTORIES[BIN_DIR] = None
    BACKEND = backend

    return

def generate():
    import argparse

    parser = argparse.ArgumentParser(description="Generate makefile and compile commands.")
    parser.add_argument("--compiledb", "-c", action="store_true",
                        help="Generate compile commands database instead of Makefile")

    args = parser.parse_args()
    if args.compiledb:
        compile_commands()
    else:
        makefile()
