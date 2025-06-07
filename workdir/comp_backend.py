import pathlib
import subprocess
import os.path
import time
import sys
import shutil
import argparse

from typing import List, Optional, TextIO, Type, Union, Tuple, Dict

BUILD_DIR: pathlib.Path
BIN_DIR: pathlib.Path
WORKDIR: pathlib.Path

Product = Union['Obj', 'Cmd', 'Exe']

DIRECTORIES: Dict[str, None] = dict()
TARGET_GROUPS: Dict[str, List[Product]] = dict()
OBJECTS: Dict[str, 'Obj'] = dict()
EXECUTABLES: Dict[str, 'Exe'] = dict()
CUSTOMS: Dict[str, 'Cmd'] = dict()

CLFLAGS: str
LINKFLAGS: str

g_context: Optional['Context'] = None

class Context:
    def __init__(self, directory: Optional[str]=None,
                 group: Optional[str]=None,
                 defines: List[Union[str, Tuple[str, str]]]=[],
                 includes: List[str]=[],
                 namespace: Optional[str]=None) -> None:
        self.group = group
        self.directory = directory and str(pathlib.PurePath(directory))
        self.defines = defines
        self.includes = includes
        self.namespace = namespace or ""

    def __enter__(self) -> 'Context':
        global g_context
        if g_context is not None:
            raise RuntimeError("Recursive Context not allowed")
        g_context = self
        return self

    def __exit__(self, t, v, tb) -> None:
        global g_context
        assert g_context is self
        g_context = None
        return

class Obj:
    def __init__(self, name: str, source: str, cmp_flags: str="", namespace: str="",
                 depends: List[Union[str, Product]]=[]) -> None:
        self.name = name
        self.namespace = namespace
        self.source = source
        self.cmp_flags=cmp_flags
        self.depends: List[str] = []
        for dep in depends:
            if isinstance(dep, str):
                self.depends.append(dep)
            else:
                self.depends.append(dep.product)

    @property
    def product(self) -> str:
        return str(BUILD_DIR / self.name)

class Cmd:
    def __init__(self, name: str, cmd: str, depends: List[Union[str, Product]], 
                 directory: str) -> None:
        self.depends: List[str] = []
        for dep in depends:
            if isinstance(dep, str):
                self.depends.append(dep)
            else:
                self.depends.append(dep.product)
        self.name = name
        self.cmd = cmd
        self.dir = directory

    @property
    def product(self) -> str:
        return str(pathlib.PurePath(self.dir) / self.name)

class Exe:
    def __init__(self, name: str, objs: List[Obj], cmds: List[Cmd],
                 link_flags: str, dll: bool, directory: str) -> None:
        self.directory = directory
        self.name = name
        self.objs = objs
        self.cmds = cmds
        self.link_flags = link_flags
        self.dll = dll

    @property
    def product(self) -> str:
        return str(pathlib.PurePath(self.directory) / self.name)

class BackendBase:
    builddir: pathlib.Path
    bindir: pathlib.Path
    workdir: pathlib.Path
    clflags: str
    linkflags: str

    @property
    def msvc(self) -> bool:
        return False
    @property
    def mingw(self) -> bool:
        return False
    @property
    def clang(self) -> bool:
        return False

class ZigCC(BackendBase):
    name = "zigcc"

    @property
    def clang(self) -> bool:
        return True

    def compile_obj(self, obj: Obj) -> str:
        return f"zig.exe cc -c -o {obj.product} {obj.cmp_flags} {obj.source}"

    def find_headers(self, obj: Obj) -> List[str]:
        cmd = f"zig.exe cc -MM {obj.cmp_flags} {obj.source}"
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            print(res)
            raise RuntimeError("Failed generating headers")
        line = res.stdout.decode().replace("\\\r\n", "")
        return line.split()[2:]

    def link_exe(self, exe: Exe) -> str:
        row = " ".join([f"{obj.product}" for obj in exe.objs] + [f"{cmd.product}" for cmd in exe.cmds])
        return f"zig.exe cc -o {exe.product} {exe.link_flags} {row}"

    def link_dll(self, exe: Exe) -> str:
        row = " ".join([f"{obj.product}" for obj in exe.objs] + [f"{cmd.product}" for cmd in exe.cmds])
        return f"zig.exe cc -shared -o {exe.product} {exe.link_flags} {row}"

    def include(self, directory: str) -> str:
        return f"-I{directory}"

    def define(self, key: str, val: Optional[str]) -> str:
        if val:
            return f"-D{key}={val}"
        return f"-D{key}"

    def import_lib_cmd(self, out: str, dll: str, symbols: List[str]) -> Cmd:
        def_path = BUILD_DIR / pathlib.PurePath(out).with_suffix(".def").name
        out_path = BUILD_DIR / out
        cmd = f"echo EXPORTS > {def_path}\n"
        for sym in symbols:
            cmd += f"\techo {sym} >> {def_path}\n"
        cmd += f"\tzig.exe dlltool -D {dll} -d {def_path} -l {out_path}"
        return Command(out, cmd)

class Mingw(BackendBase):
    name = "mingw"

    @property
    def mingw(self) -> bool:
        return True
    
    def compile_obj(self, obj: Obj) -> str:
        return f"gcc.exe -c -o {obj.product} {obj.cmp_flags} {obj.source}"

    def find_headers(self, obj: Obj) -> List[str]:
        cmd = f"gcc.exe -MM {obj.cmp_flags} {obj.source}"
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            print(res)
            raise RuntimeError("Failed generating headers")
        line = res.stdout.decode().replace("\\\r\n", "")
        return line.split()[2:]

    def link_exe(self, exe: Exe) -> str:
        row = " ".join([f"{obj.product}" for obj in exe.objs] + [f"{cmd.product}" for cmd in exe.cmds])
        return f"gcc.exe -o {exe.product} {exe.link_flags} {row}"

    def link_dll(self, exe: Exe) -> str:
        row = " ".join([f"{obj.product}" for obj in exe.objs] + [f"{cmd.product}" for cmd in exe.cmds])
        return f"gcc.exe -shared -o {exe.product} {exe.link_flags} {row}"

    def include(self, directory: str) -> str:
        return f"-I{directory}"

    def define(self, key: str, val: Optional[str]) -> str:
        if val:
            return f"-D{key}={val}"
        return f"-D{key}"

    def import_lib_cmd(self, out: str, dll: str, symbols: List[str]) -> Cmd:
        def_path = BUILD_DIR / pathlib.PurePath(out).with_suffix(".def").name
        out_path = BUILD_DIR / out
        cmd = f"echo EXPORTS > {def_path}\n"
        for sym in symbols:
            cmd += f"\techo {sym} >> {def_path}\n"
        cmd += f"\tdlltool --dllname {dll} --def {def_path} --output-lib {out_path}"
        return Command(out, cmd)


class Msvc(BackendBase):
    name = "msvc"

    @property
    def msvc(self) -> bool:
        return True

    def compile_obj(self, obj: Obj) -> str:
        return f"cl.exe /c /Fo:{obj.product} {obj.cmp_flags} {obj.source}"

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
        row = " ".join([f"{obj.product}" for obj in exe.objs] + [f"{cmd.product}" for cmd in exe.cmds])
        return f"link /OUT:{exe.product} {exe.link_flags} {row}"

    def link_dll(self, exe: Exe) -> str:
        row = " ".join([f"{obj.product}" for obj in exe.objs] + [f"{cmd.product}" for cmd in exe.cmds])
        return f"link /OUT:{exe.product} /DLL {exe.link_flags} {row}"

    def include(self, directory: str) -> str:
        return f"/I{directory}"

    def define(self, key: str, val: Optional[str]) -> str:
        if val:
            return f"/D{key}={val}"
        return f"/D{key}"

    def import_lib_cmd(self, out: str, dll: str, symbols: List[str]) -> Cmd:
        exports = " ".join([f"/EXPORT:{s}={s}" for s in symbols])
        cmd = f"lib /NOLOGO /MACHINE:X64 /DEF /OUT:{BUILD_DIR / out} /NAME:{dll} {exports}"
        return Command(out, cmd)


def Object(name: str, source: str, cmp_flags: Optional[str]=None, namespace: str="",
           depends: List[Union[str, Product]]=[],
           defines: List[Union[str, Tuple[str, str]]]=[],
           includes: List[str]=[], extra_cmp_flags: Optional[str]=None,
           context: Optional[Context]=g_context) -> Obj:
    if context is None:
        context = g_context
    if cmp_flags is None:
        cmp_flags = CLFLAGS
    if context is not None:
        defines = defines + context.defines
        includes = includes + context.includes
        if not namespace:
            namespace = context.namespace
    if extra_cmp_flags is not None:
        cmp_flags += " " + extra_cmp_flags
    for define in defines:
        if isinstance(define, tuple):
            key, val = define
        else:
            key, val = define, None
        cmp_flags += " " + BACKEND.define(key, val)
    for directory in includes:
        cmp_flags += " " + BACKEND.include(str(pathlib.PurePath(directory)))
        
    source = str(pathlib.PurePath(source))
    if namespace:
        name = str(pathlib.PurePath(namespace) / name)
    obj = Obj(name, source, cmp_flags, namespace, depends)

    if namespace:
        DIRECTORIES[str(BUILD_DIR / namespace)] = None

    if name in OBJECTS:
        other = OBJECTS[name]
        if other.cmp_flags == obj.cmp_flags and other.source == obj.source:
            return other
        raise RuntimeError(f"Duplicate object {name}")
    OBJECTS[name] = obj
    return obj


def Executable(name: str, *sources: Union[str, Obj, Cmd], 
               cmp_flags: Optional[str]=None, link_flags: Optional[str]=None,
               namespace: str="", dll: bool=False,
               extra_link_flags: Optional[str]=None,
               defines: List[Union[str, Tuple[str, str]]]=[],
               includes: List[str]=[],
               directory: Optional[str]=None, 
               group: Optional[str]=None,
               context: Optional[Context]=None) -> Exe:
    if context is None:
        context = g_context
    if cmp_flags is None:
        cmp_flags = CLFLAGS
    if link_flags is None:
        link_flags = LINKFLAGS
    if extra_link_flags is not None:
        link_flags += " " + extra_link_flags

    objs = []
    cmds = []
    for src in sources:
        if isinstance(src, str):
            obj_name = str(pathlib.PurePath(src).with_suffix('.obj').name)
            obj = Object(obj_name, src, cmp_flags, namespace, defines=defines,
                         includes=includes, context=context)
        else:
            if isinstance(src, Cmd):
                if src not in cmds:
                    cmds.append(src)
                continue
            assert isinstance(src, Obj)
            obj = src
        if obj not in objs:
            objs.append(obj)
    if directory is None:
        if context is not None:
            directory = context.directory or str(BIN_DIR)
        else:
            directory = str(BIN_DIR)
    else:
        directory = str(pathlib.PurePath(directory))
    if group is None:
        if context is not None:
            group = context.group or "all"
        else:
            group = "all"
    exe = Exe(name, objs, cmds, link_flags, dll, directory)
    if exe.product in EXECUTABLES:
        raise RuntimeError(f"Duplicate exe {exe.product}")
    EXECUTABLES[exe.product] = exe
    if directory is not None:
        DIRECTORIES[directory] = None
    if group not in TARGET_GROUPS:
        TARGET_GROUPS[group] = [exe]
    else:
        TARGET_GROUPS[group].append(exe)
    return exe

def Command(name: str, cmd: str, *depends: Union[str, Product],
            directory: Optional[str]=None) -> Cmd:
    if directory is None:
        directory = str(BUILD_DIR)
    else:
        directory = str(pathlib.PurePath(directory))
    if name in CUSTOMS:
        raise RuntimeError(f"Duplicate command {name}")
    DIRECTORIES[directory] = None
    command = Cmd(name, cmd, list(depends), directory)
    CUSTOMS[name] = command
    if directory == str(BIN_DIR):
        TARGET_GROUPS["all"].append(command)
    return command

def CopyToBin(*src: str) -> List[Cmd]:
    res = []
    for file in src:
        f = pathlib.PurePath(file)
        name = pathlib.PurePath(file).name
        res.append(Command(name, f"type {f} > {BIN_DIR}\\{name}", str(f),
                           directory=str(BIN_DIR)))
    return res


def ImportLib(lib: str, dll: str, symbols: List[str]) -> Cmd:
    return BACKEND.import_lib_cmd(lib, dll, symbols)

Backend = Union[Msvc, Mingw, ZigCC]
BACKEND: Backend

def define(key: str, val: Optional[str]=None) -> str:
    return BACKEND.define(key, val)

def find_headers(obj: Obj) -> List[str]:
    import json
    data = None
    try:
        with open('comp_cache.json', 'r') as file:
            data = json.load(file)
            cache = data[BACKEND.name][obj.name]
            assert 'stamp' in cache and isinstance(cache['stamp'], float)
            assert 'headers' in cache and isinstance(cache['headers'], list)
        if get_args().fast:
            return cache['headers']
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
    created: List[pathlib.Path] = []
    for dep in obj.depends:
        path = pathlib.Path(dep)
        if path.suffix in [".h", ".hpp", ".inl"]:
            if not path.exists():
                try:
                    path.touch()
                    created.append(path)
                except Exception:
                    pass

    try:
        headers = BACKEND.find_headers(obj)
        for ix in range(len(headers)):
            header = headers[ix]
            try:
                header = str(pathlib.Path(header).resolve().relative_to(WORKDIR))
                headers[ix] = header
            except Exception:
                pass
    finally:
        for path in created:
            try:
                path.unlink()
            except Exception:
                pass


    if data is None:
        data = dict()

    if BACKEND.name not in data:
        data[BACKEND.name] = dict()
    data[BACKEND.name][obj.name] = {'stamp': time.time(), 'headers': headers}
    try:
        with open('comp_cache.json', 'w') as file:
            json.dump(data, file, indent=4)
    except Exception:
        pass
    return headers
        
def makefile(dest: TextIO = sys.stdout) -> None:
    for obj in OBJECTS.values():
        p = pathlib.Path(obj.source).resolve()
        if not p.is_file():
            raise RuntimeError(f"File '{p}' does not exist")

    for group, targets in TARGET_GROUPS.items():
        print(f"{group}:", end="", file=dest)
        for prod in targets:
            print(" \\", file=dest)
            print(f"\t{prod.product}", end="", file=dest)
        print("\n", file=dest)

    dirs = list(DIRECTORIES.keys())

    while len(dirs) > 0:
        d = dirs.pop()
        path = pathlib.PurePath(d)
        parents = [str(p) for p in path.parents[:-1]]
        for parent in parents:
            DIRECTORIES[parent] = None
        dirs.extend(parents)
    for d in DIRECTORIES:
        path = pathlib.PurePath(d)
        parents = [str(p) for p in path.parents[:-1]]
        if len(parents) > 0:
            parent_str = " " + " ".join(parents)
        else:
            parent_str = ""
        print(f"{path}:{parent_str}\n\t-@ if NOT EXIST \"{path}\" mkdir \"{path}\"", file=dest)

    print(file=dest)

    for key, obj in OBJECTS.items():
        if obj.namespace:
            bld_dir = str(BUILD_DIR / obj.namespace)
        else:
            bld_dir = str(BUILD_DIR)
        headers = find_headers(obj)
        depends = " ".join([obj.source] + headers + [bld_dir] + obj.depends)
        print(f"{obj.product}: {depends}", file=dest)
        print(f"\t{BACKEND.compile_obj(obj)}", file=dest)
    print(file=dest)
    for key, exe in EXECUTABLES.items():
        row = " ".join([f"{obj.product}" for obj in exe.objs] + [f"{cmd.product}" for cmd in exe.cmds])
        print(f"{exe.product}: {row} {exe.directory}", file=dest)
        if exe.dll:
            print(f"\t{BACKEND.link_dll(exe)}", file=dest)
        else:
            print(f"\t{BACKEND.link_exe(exe)}", file=dest)
    print(file=dest)
    for key, cmd in CUSTOMS.items():
        row = " ".join(cmd.depends + [cmd.dir])
        print(f"{cmd.product}: {row}", file=dest)
        print(f"\t{cmd.cmd}", file=dest)
    print(file=dest)
    print(f"clean:\n\tdel /S /Q {BUILD_DIR}\\*\n\tdel /S /Q {BIN_DIR}\\*", file=dest)

def compile_commands(dest: TextIO = sys.stdout) -> None:
    directory = str(WORKDIR.resolve())
    res = []
    for obj in OBJECTS.values():
        json = dict()
        json["directory"] = directory
        json["command"] = BACKEND.compile_obj(obj)
        json["file"] = str(pathlib.Path(obj.source).resolve())
        res.append(json)
    import json
    print(json.dumps(res, indent=4), file=dest)

all_backends: Dict[str, Type['Backend']] = {"msvc": Msvc, "mingw": Mingw, 'zigcc': ZigCC}
active_backends: Dict[str, 'Backend'] = {}

args = None
parser = None

def get_parser() -> 'argparse.ArgumentParser':
    global parser
    if parser is None:
        parser = argparse.ArgumentParser(description="Generate makefile and compile commands.")
        parser.add_argument("--compiledb", "-c", action="store_true",
                            help="Generate compile commands database instead of Makefile")
        parser.add_argument("--backend", "-b", choices = [key for key in active_backends],
                            type=str.lower,
                            default=None, help="Specify the backend to use.")
        parser.add_argument("--target", "-t", help="Specify target to build", action="store", 
                            default="all")
        parser.add_argument("--fast", "-f", help="Speed up by not validating header cache", 
                            action="store_true", default=False)
    return parser


def get_args() -> 'argparse.Namespace':
    global args
    if args is None:
        parser = get_parser()
        args = parser.parse_args()
    return args


def add_backend(name: str, backend_name: str, builddir: str, bindir: str, 
                workdir: pathlib.Path, clflags: str, linkflags: str) -> None:
    if backend_name.lower() not in all_backends:
        raise RuntimeError("Invalid backend")
    backend = all_backends[name.lower()]()
    backend.builddir = pathlib.Path(builddir)
    backend.bindir = pathlib.Path(bindir)
    backend.workdir = workdir
    backend.clflags = clflags
    backend.linkflags = linkflags
    active_backends[name.lower()] = backend


def set_backend(name: str) -> None:
    global BUILD_DIR, BIN_DIR, WORKDIR, CLFLAGS, LINKFLAGS, BACKEND
    backend = get_args().backend
    if backend is not None:
        name = backend
    if name.lower() not in active_backends:
        raise RuntimeError("Invalid backend")
    backend = active_backends[name.lower()]
    BUILD_DIR = backend.builddir
    BIN_DIR = backend.bindir
    WORKDIR = backend.workdir
    CLFLAGS = backend.clflags
    LINKFLAGS = backend.linkflags
    DIRECTORIES[str(BUILD_DIR)] = None
    DIRECTORIES[str(BIN_DIR)] = None
    TARGET_GROUPS["all"] = []
    BACKEND = backend

    return

def backend() -> Backend:
    return BACKEND
def build_dir() -> str:
    return str(BUILD_DIR)
def bin_dir() -> str:
    return str(BIN_DIR)

def generate() -> None:
    if get_args().compiledb:
        compile_commands()
    else:
        makefile()

def get_make() -> str:
    make = shutil.which('nmake.exe')
    if make is None:
        raise RuntimeError("Could not find nmake exe")
    return make

def build(comp_file: str) -> None:
    comp_stamp = os.path.getmtime(pathlib.Path(comp_file))
    compiledb = WORKDIR / 'compile_commands.json'
    make = BUILD_DIR / 'Makefile'
    try:
        if not compiledb.exists() or os.path.getmtime(compiledb) < comp_stamp or get_args().compiledb:
            file_open = False
            try:
                with open(compiledb, 'w') as file:
                    file_open = True
                    compile_commands(dest=file)
            except:
                if file_open:
                    compiledb.unlink()
                raise
        if not make.exists() or os.path.getmtime(make) < comp_stamp:
            file_open = False
            try:
                with open(make, 'w') as file:
                    file_open = True
                    makefile(dest=file)
            except:
                if file_open:
                    make.unlink()
                raise
    except Exception as e:
        print(f"Failed building: {e}", file=sys.stderr)
        exit(1)

    make_exe = get_make()
    res = subprocess.run([make_exe, '-f', str(make), '/NOLOGO', get_args().target])

    exit(res.returncode)

