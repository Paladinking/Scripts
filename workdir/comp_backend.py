import pathlib
import subprocess
import os.path
import time
import sys
import shutil
import argparse

from typing import Any, List, Optional, TextIO, Type, Union, Tuple, Dict

BUILD_DIR: pathlib.Path
BIN_DIR: pathlib.Path
WORKDIR: pathlib.Path

Product = Union['Obj', 'Cmd', 'Exe']

DIRECTORIES: Dict[str, None] = dict()
TARGET_GROUPS: Dict[str, List[Product]] = dict()
OBJECTS: Dict[str, 'Obj'] = dict()
EXECUTABLES: Dict[str, 'Exe'] = dict()
CUSTOMS: Dict[str, 'Cmd'] = dict()

use_copyto = False

CLFLAGS: str
LINKFLAGS: str

g_context: Optional['Context'] = None


class Package:
    def __init__(self, compile_flags: str, link_flags: str, dlls: List[str],
                 sources: List[str]) -> None:
        self.compile_flags = compile_flags
        self.link_flags = link_flags
        self.dlls = dlls
        self.sources = sources


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
        self.cmd = "<unkown>"
        self.headers: Optional[Dict[str, None]] = None
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
        self.rule: Optional[str] = None

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
        self.cmd = '<unkown>'
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

    def handle_obj_line(self, obj: Obj, line: str) -> bool:
        return False

    def compile_obj(self, obj: Obj) -> str:
        return f"zig.exe cc -c -o {obj.product} {obj.cmp_flags} {obj.source}"

    def compile_obj_ninja(self) -> str:
        return "    deps = gcc\n    depfile = $out.d\n" + \
               "    command = zig.exe cc -MMD -MF $out.d -c $in -o $out $cl_flags"

    def link_exe_ninja(self) -> str:
        return "    command = zig.exe cc -o $out $in $link_flags"

    def link_dll_ninja(self) -> str:
        return "    command = zig.exe cc -shared -o $out $in $link_flags"

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

    def libpath(self, directory: str) -> str:
        return f"-L{directory}"

    def define(self, key: str, val: Optional[str]) -> str:
        if val:
            return f"-D{key}={val}"
        return f"-D{key}"

    def import_lib_cmd(self, out: str, dll: str, symbols: List[str]) -> Cmd:
        def_path = BUILD_DIR / pathlib.PurePath(out).with_suffix(".def").name
        def_path = def_path.relative_to(BUILD_DIR)
        def_file = Command(str(def_path), f"python comp.py --deffile {def_path} {' '.join(symbols)}")
        out_path = BUILD_DIR / out
        cmd = f"zig.exe dlltool -D {dll} -d {def_path} -l {out_path}"
        return Command(out, cmd, def_file)

class Mingw(BackendBase):
    name = "mingw"

    def __init__(self) -> None:
        self._pending_line = False

    @property
    def mingw(self) -> bool:
        return True

    def handle_obj_line(self, obj: Obj, line: str) -> bool:
        if obj.headers is None:
            obj.headers = dict()
        pref = f"{obj.product}:"
        offset = 0
        if line.startswith(pref):
            line = line[len(pref):]
            offset = 1
        elif not self._pending_line:
            return False
        self._pending_line = False

        if line.endswith("\\"):
            self._pending_line = True
            line = line[:-2]

        for h in line.split()[offset:]:
            try:
                header = str(pathlib.Path(h).resolve().relative_to(WORKDIR))
                obj.headers[header] = None
            except Exception:
                pass

        return True
    
    def compile_obj(self, obj: Obj) -> str:
        return f"gcc.exe -MMD -MF - -c -o {obj.product} {obj.cmp_flags} {obj.source}"
    
    def compile_obj_ninja(self) -> str:
        return "    deps = gcc\n    depfile = $out.d\n" + \
               "    command = gcc.exe -MMD -MF $out.d -c $in -o $out $cl_flags"

    def link_exe_ninja(self) -> str:
        return "    command = gcc.exe -o $out $in $link_flags"

    def link_dll_ninja(self) -> str:
        return "    command = gcc.exe -shared -o $out $in $link_flags"

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

    def libpath(self, directory: str) -> str:
        return f"-L{directory}"

    def define(self, key: str, val: Optional[str]) -> str:
        if val:
            return f"-D{key}={val}"
        return f"-D{key}"

    def import_lib_cmd(self, out: str, dll: str, symbols: List[str]) -> Cmd:
        def_path = BUILD_DIR / pathlib.PurePath(out).with_suffix(".def").name
        def_path = def_path.relative_to(BUILD_DIR)
        def_file = Command(str(def_path), f"python comp.py --deffile {def_path} {' '.join(symbols)}")
        out_path = BUILD_DIR / out
        cmd = f"dlltool.exe -D {dll} -d {def_path} -l {out_path}"
        return Command(out, cmd, def_file)


class Msvc(BackendBase):
    name = "msvc"

    @property
    def msvc(self) -> bool:
        return True

    def handle_obj_line(self, obj: Obj, line: str) ->  bool:
        if obj.headers is None:
            obj.headers = dict()
        if line.startswith("Note: including file"):
            path = pathlib.Path(line[21:].strip()).resolve()
            try:
                path = path.relative_to(WORKDIR)
                obj.headers[str(path)] = None
            except ValueError:
                pass
            return True
        return False

    def compile_obj(self, obj: Obj) -> str:
        return f"cl.exe /c /nologo /showIncludes /Fo:{obj.product} {obj.cmp_flags} {obj.source}"

    def compile_obj_ninja(self) -> str:
        return "    deps = msvc\n" + \
        "    command = cl.exe /nologo /showIncludes /c $in /Fo:$out $cl_flags"

    def link_exe_ninja(self) -> str:
        return "    command = link.exe /nologo /OUT:$out $link_flags $in"

    def link_dll_ninja(self) -> str:
        return "    command = link.exe /nologo /OUT:$out /DLL $link_flags $in"

    def find_headers(self, obj: Obj) -> List[str]:
        cl = shutil.which("cl.exe")
        if cl is None:
            raise RuntimeError("Failed to find cl.exe executable")
        cmd = f"{cl} /P /showIncludes /FiNUL {obj.cmp_flags} {obj.source}"
        print(cmd)
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
        return f"link /nologo /OUT:{exe.product} {exe.link_flags} {row}"

    def link_dll(self, exe: Exe) -> str:
        row = " ".join([f"{obj.product}" for obj in exe.objs] + [f"{cmd.product}" for cmd in exe.cmds])
        return f"link /nologo /OUT:{exe.product} /DLL {exe.link_flags} {row}"

    def include(self, directory: str) -> str:
        return f"/I{directory}"

    def libpath(self, directory: str) -> str:
        return f"/LIBPATH:{directory}"

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
               packages: List[Package]=[],
               context: Optional[Context]=None) -> Exe:
    if context is None:
        context = g_context
    if cmp_flags is None:
        cmp_flags = CLFLAGS
    if link_flags is None:
        link_flags = LINKFLAGS
    if extra_link_flags is not None:
        link_flags += " " + extra_link_flags
    for pkg in packages:
        if pkg.compile_flags:
            cmp_flags += " " + pkg.compile_flags
        if pkg.link_flags:
            link_flags += " " + pkg.link_flags
        if pkg.sources:
            sources = sources + tuple(pkg.sources)

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
    global use_copyto
    use_copyto = True
    res = []
    for file in src:
        f = pathlib.PurePath(file)
        name = pathlib.PurePath(file).name
        dest = BIN_DIR / name
        cmd = Command(name, f"python comp.py --copyto {f} {dest}", str(f),
                      directory=str(BIN_DIR))
        cmd.rule = "copyto"
        res.append(cmd)
    return res


def ImportLib(lib: str, dll: str, symbols: List[str]) -> Cmd:
    return BACKEND.import_lib_cmd(lib, dll, symbols)

Backend = Union[Msvc, Mingw, ZigCC]
BACKEND: Backend
ACTIVE_BACKEND: str

def define(key: str, val: Optional[str]=None) -> str:
    return BACKEND.define(key, val)

COMPCACHE: Optional[dict[str, Any]] = None

def comp_cache() -> dict[str, Any]:
    global COMPCACHE
    if COMPCACHE is not None:
        return COMPCACHE
    import json
    try:
        with open(WORKDIR / 'comp_cache.json', 'r') as file:
            COMPCACHE = json.load(file)
    except Exception:
        COMPCACHE = dict()
    return COMPCACHE # type: ignore

def write_comp_cache() -> None:
    try:
        import json
        with open(WORKDIR / 'comp_cache.json', 'w') as file:
            file.write(json.dumps(COMPCACHE, indent=4))
    except Exception as e:
        print(f"Failed writing cache: {e}", file=sys.stderr)

def set_headers_and_last_cmd(obj: Obj) -> None:
    try:
        data = comp_cache()
        if BACKEND.name not in data:
            data[BACKEND.name] = dict()
        if obj.name not in data[BACKEND.name]:
            data[BACKEND.name][obj.name] = {"cmd": obj.cmd, "headers": obj.headers}
        else:
            data[BACKEND.name][obj.name]['cmd'] = obj.cmd
            data[BACKEND.name][obj.name]['headers'] = obj.headers and list(obj.headers.keys())
    except Exception as e:
        print(f"Failed updating cache: {e}", file=sys.stderr)


def set_last_cmd(obj: Product) -> None:
    try:
        data = comp_cache()
        if BACKEND.name not in data:
            data[BACKEND.name] = dict()
        if obj.name not in data[BACKEND.name]:
            data[BACKEND.name][obj.name] = {"cmd": obj.cmd}
        else:
            data[BACKEND.name][obj.name]['cmd'] = obj.cmd
    except Exception as e:
        print(f"Failed updating cache: {e}", file=sys.stderr)

def last_cmd(prod: Product) -> Optional[str]:
    try:
        data = comp_cache()
        cache = data[BACKEND.name][prod.name]
        if isinstance(cache['cmd'], str):
            return cache['cmd']
    except Exception:
        pass
    return None

def find_headers(obj: Obj) -> Optional[Dict[str, None]]:
    data = None
    try:
        data = comp_cache()
        cache = data[BACKEND.name][obj.name]
        #assert 'stamp' in cache and isinstance(cache['stamp'], float)
        assert 'headers' in cache and isinstance(cache['headers'], list)
        return {h: None for h in cache['headers']}
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
    return None
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

def resolve_dirs() -> None:
    dirs = list(DIRECTORIES.keys())

    while len(dirs) > 0:
        d = dirs.pop()
        path = pathlib.PurePath(d)
        parents = [str(p) for p in path.parents[:-1]]
        for parent in parents:
            DIRECTORIES[parent] = None
        dirs.extend(parents)

def ninjafile(dest: TextIO = sys.stdout) -> None:
    resolve_dirs()

    def esc(s : Union[str, pathlib.Path]) -> str:
        return str(s).replace(" ", "$ ").replace(":", "$:")

    print("ninja_required_version = 1.5", file=dest)
    print(f"builddir = {esc(BACKEND.builddir)}\n", file=dest)
    print(f"link_flags = {LINKFLAGS}\n", file=dest)


    for group, targets in TARGET_GROUPS.items():
        deps = " ".join(f'{esc(p.product)}' for p in targets)
        print(f"build {group}: phony {deps}\n", file=dest)

    if len(OBJECTS) > 0:
        print(f"cl_flags = {CLFLAGS}", file=dest)
        print(f"rule cl", file=dest)
        print(BACKEND.compile_obj_ninja(), file=dest)

    if len(EXECUTABLES) > 0:
        print(f"link_flags = {LINKFLAGS}", file=dest)
        print("rule link_exe", file=dest)
        print(BACKEND.link_exe_ninja(), file=dest)
        print("rule link_dll", file=dest)
        print(BACKEND.link_dll_ninja(), file=dest)

    if use_copyto:
        print(f"rule copyto", file=dest)
        print("    command = python comp.py --copyto $in $out", file=dest)
    print(file=dest)


    for key, obj in OBJECTS.items():
        print(f"build {esc(obj.product)}: cl {esc(obj.source)}", end="", file=dest)
        if obj.depends:
            deps = ' '.join((esc(d) for d in obj.depends))
            print(f" || {deps}", file=dest)
        else:
            print(file=dest)
        if obj.cmp_flags != CLFLAGS:
            print(f"    cl_flags = {obj.cmp_flags}", file=dest)
    print(file=dest)
    for key, exe in EXECUTABLES.items():
        row = " ".join([f"{esc(obj.product)}" for obj in exe.objs] + 
                       [f"{esc(cmd.product)}" for cmd in exe.cmds])
        if exe.dll:
            print(f"build {esc(exe.product)}: link_dll {row}", file=dest)
        else:
            print(f"build {esc(exe.product)}: link_exe {row}", file=dest)
        if exe.link_flags != LINKFLAGS:
            print(f"    link_flags = {exe.link_flags}", file=dest)
    print(file=dest)
    ix = 0
    for key, obj in CUSTOMS.items():
        if obj.rule is not None:
            rule = obj.rule
        else:
            rule = f"custom_{ix}"
            ix += 1
            print(f"rule {rule}", file=dest)
            print(f"    command = {obj.cmd}", file=dest)
        deps = ' '.join(esc(d) for d in obj.depends)
        print(f"build {esc(obj.product)}: {rule} {deps}", file=dest)

        
def makefile(dest: TextIO = sys.stdout) -> None:
    resolve_dirs()

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
        if obj.headers is None or last_cmd(obj) != obj.cmd:
            headers = [str(BUILD_DIR / "Makefile")]
        else:
            headers = list(obj.headers.keys())
        depends = " ".join([obj.source] + headers + [bld_dir] + obj.depends)
        print(f"{obj.product}: {depends}", file=dest)
        print(f"\t{obj.cmd}", file=dest)
    print(file=dest)
    for key, exe in EXECUTABLES.items():
        row = " ".join([f"{obj.product}" for obj in exe.objs] + 
                       [f"{cmd.product}" for cmd in exe.cmds] + 
                       ([str(BUILD_DIR / "Makefile")] if last_cmd(exe) != exe.cmd else []))
        print(f"{exe.product}: {row} {exe.directory}", file=dest)
        print(f"\t{exe.cmd}", file=dest)
    print(file=dest)
    for key, cmd in CUSTOMS.items():
        row = " ".join(cmd.depends + [cmd.dir] + ([str(BUILD_DIR / "Makefile")] 
                       if last_cmd(cmd) != cmd.cmd else []))
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
        parser.add_argument("--deffile", help="Generate a .def file", action="store",
                            nargs="+", default=None)
        parser.add_argument("--copyto", help="Copy file", action="store",
                            nargs="+", default=None)
        parser.add_argument("--make", help="Use nmake instead of ninja", action="store_true",
                            default=False)
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
    global BUILD_DIR, BIN_DIR, WORKDIR, CLFLAGS, LINKFLAGS, BACKEND, ACTIVE_BACKEND
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
    ACTIVE_BACKEND = name

    run = True
    if get_args().deffile:
        run = False
        args = get_args().deffile
        dest = args[0]
        with open(dest, 'w') as file:
            print("EXPORTS", file=file)
            for arg in args[1:]:
                print(arg, file=file)

    elif get_args().copyto:
        run = False
        args = get_args().copyto
        if len(args) != 2:
            print("Invalid copyto args", file=sys.stderr)
            exit(1)
        shutil.copy2(args[0], args[1])

    if not run:
        exit(0)

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

def get_make() -> Tuple[str, List[str]]:
    if get_args().make:
        s = 'nmake.exe'
        args = ['/NOLOGO', get_args().target]
    else:
        s = 'ninja.exe'
        if get_args().target == 'clean':
            args = ['-t', 'clean']
        else:
            args = [get_args().target]
    make = shutil.which(s)
    if make is None:
        raise RuntimeError(f"Could not find {s}")
    return make, args

def build(comp_file: str) -> None:

    comp_stamp = os.path.getmtime(pathlib.Path(comp_file))
    compiledb = WORKDIR / 'compile_commands.json'

    if get_args().make:
        make = BUILD_DIR / 'Makefile'
    else:
        make = BUILD_DIR / 'build.ninja'


    cmd_map = dict()
    
    if get_args().make:
        for obj in OBJECTS.values():
            obj.headers = find_headers(obj)
            obj.cmd = BACKEND.compile_obj(obj)
            cmd_map[obj.cmd] = obj
        for exe in EXECUTABLES.values():
            if exe.dll:
                exe.cmd = BACKEND.link_dll(exe)
            else:
                exe.cmd = BACKEND.link_exe(exe)
            cmd_map[exe.cmd] = exe
        for c in CUSTOMS.values():
            cmd_map[c.cmd] = c

    try:
        if not BUILD_DIR.exists():
            os.mkdir(BUILD_DIR)

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
            print("Regenerating Makefile")
            file_open = False
            try:
                with open(make, 'w') as file:
                    file_open = True
                    if get_args().make:
                        makefile(dest=file)
                    else:
                        ninjafile(dest=file)
            except:
                if file_open:
                    make.unlink()
                raise
    except Exception as e:
        print(f"Failed building: {e}", file=sys.stderr)
        exit(1)

    make_exe, args = get_make()
    if not get_args().make:
        res = subprocess.run([make_exe, '-f', str(make), *args])
        if res.returncode != 0:
            exit(res.returncode)
        return
    res = subprocess.Popen([make_exe, '-f', str(make), *args], bufsize=1,
                           universal_newlines=True, stdout=subprocess.PIPE)
    assert res.stdout is not None

    cur_obj = None

    change = False

    for line in res.stdout:
        l = line.strip()
        if l in cmd_map:
            cur_obj = cmd_map[l]
            if not isinstance(cur_obj, Obj):
                cur_obj = None
            else:
                cur_obj.headers = dict()
        elif isinstance(cur_obj, Obj):
            if BACKEND.handle_obj_line(cur_obj, l):
                change = True
                continue

        print(line, end="")

    res.wait()

    if res.returncode == 0:
        for obj in OBJECTS.values():
            set_headers_and_last_cmd(obj)
        for exe in EXECUTABLES.values():
            set_last_cmd(exe)
        for cmd in CUSTOMS.values():
            set_last_cmd(cmd)

        write_comp_cache()
        
        if change:
            try:
                with open(make, 'w') as file:
                    file_open = True
                    makefile(dest=file)
            except Exception as e:
                print(f"Failed generating makefile: {e}", file=sys.stderr)
    if res.returncode != 0:
        exit(res.returncode)


def find_cmake() -> str:
    cmake = shutil.which('cmake.exe')
    if cmake is None:
        raise RuntimeError("Could not find cmake.exe")
    res = subprocess.run(['cmake', '--version'], stdout=subprocess.PIPE)
    if res.returncode != 0:
        raise RuntimeError("Bad cmake version")
    ver = res.stdout.decode('utf8')
    if not ver.startswith('cmake version '):
        raise RuntimeError("Bad cmake version")
    ver = ver[14:]
    parts = ver.strip().split('.')
    if len(parts) < 2:
        raise RuntimeError("Bad cmake version")
    try:
        major = int(parts[0])
        minor = int(parts[1])
        if (major, minor) < (3, 22):
            raise RuntimeError("Insufficient cmake version")
    except Exception:
        raise RuntimeError("Bad cmake version")
    return cmake


def _sdl3_gfx_hook(path: pathlib.Path, pkg: Dict[str, Any]) -> None:
    root = path / "SDL3_gfx-1.0.1"
    include = root / 'include'
    if not include.is_dir():
        include.mkdir()
    include_gfx = include / 'SDL3_gfx'
    if not include_gfx.is_dir():
        include_gfx.mkdir()

    import glob

    for file in glob.glob(str(root) + "/*.h"):
        p = pathlib.Path(file)
        shutil.copy2(p, include_gfx / p.name)


def _box2d_hook(path: pathlib.Path, pkg: Dict[str, Any]) -> None:
    src_path = path.with_name(path.name + "-src")
    
    if src_path.exists():
        shutil.rmtree(src_path)
    
    shutil.move(path, src_path)

    cmake_src_path = src_path / 'box2d-3.1.1'

    build_dir = src_path / 'build'
    cmake = find_cmake()

    if BACKEND.name == "msvc":
        cmp = "cl.exe"
    else:
        cmp = "gcc.exe"

    res = subprocess.run([cmake, '-B', str(build_dir), '-S', str(cmake_src_path), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                          f'-DCMAKE_INSTALL_PREFIX={path}', '-DBOX2D_SAMPLES=OFF', '-DBOX2D_UNIT_TESTS=OFF', '-DBUILD_SHARED_LIBS=OFF',
                           f'-DCMAKE_C_COMPILER={cmp}'])
    if res.returncode != 0:
        raise RuntimeError("Failed building box2d")
    res = subprocess.run([cmake, '--build', str(build_dir)])
    if res.returncode != 0:
        raise RuntimeError("Failed building box2d")
    res = subprocess.run([cmake, '--install', str(build_dir)])
    if res.returncode != 0:
        raise RuntimeError("Failed building box2d")
    shutil.rmtree(src_path)


KNOWN_PACKAGES = {
    "box2d": {
        "msvc": {
            "url": "https://github.com/erincatto/box2d/archive/refs/tags/v3.1.1.zip",
            "include": ["include"],
            "libpath": ["lib"],
            "libname": ["box2d.lib"],
            "dll": [],
            "hook": _box2d_hook
        },
        "mingw": {
            "url": "https://github.com/erincatto/box2d/archive/refs/tags/v3.1.1.zip",
            "include": ["include"],
            "libpath": ["lib"],
            "libname": ["-lbox2d"],
            "dll": [],
            "hook": _box2d_hook
        }
    },
    "SDL3_gfx": {
        "msvc": {
            "url": "https://github.com/sabdul-khabir/SDL3_gfx/archive/refs/tags/v1.0.1.zip",
            "include": ["SDL3_gfx-1.0.1/include"],
            "libpath": [],
            "libname": [],
            "dll": [],
            "sources": ["SDL3_gfx-1.0.1/SDL3_gfxPrimitives.c", "SDL3_gfx-1.0.1/SDL3_framerate.c",
                        "SDL3_gfx-1.0.1/SDL3_imageFilter.c", "SDL3_gfx-1.0.1/SDL3_rotozoom.c"],
            "hook": _sdl3_gfx_hook
        },
        "mingw": {
            "url": "https://github.com/sabdul-khabir/SDL3_gfx/archive/refs/tags/v1.0.1.zip",
            "include": ["SDL3_gfx-1.0.1/include"],
            "libpath": [],
            "libname": [],
            "dll": [],
            "sources": ["SDL3_gfx-1.0.1/SDL3_gfxPrimitives.c", "SDL3_gfx-1.0.1/SDL3_framerate.c",
                        "SDL3_gfx-1.0.1/SDL3_imageFilter.c", "SDL3_gfx-1.0.1/SDL3_rotozoom.c"],
            "hook": _sdl3_gfx_hook
        },
    },
    "SDL3": {
        "msvc": {
            "url": "https://github.com/libsdl-org/SDL/releases/download/release-3.2.16/SDL3-devel-3.2.16-VC.zip", 
            "include": ["SDL3-3.2.16/include"],
            "libpath": ["SDL3-3.2.16/lib/x64"],
            "libname": ["SDL3.lib"],
            "dll": ["SDL3-3.2.16/lib/x64/SDL3.dll"]
        },
        "mingw": { 
            "url": "https://github.com/libsdl-org/SDL/releases/download/release-3.2.16/SDL3-devel-3.2.16-mingw.zip",
            "include": ["SDL3-3.2.16/x86_64-w64-mingw32/include"],
            "libpath": ["SDL3-3.2.16/x86_64-w64-mingw32/lib"],
            "libname": ["-lSDL3"],
            "dll": ["SDL3-3.2.16/x86_64-w64-mingw32/bin/SDL3.dll"]
        }
    },
    "SDL3_image": {
        "msvc": {
            "url": "https://github.com/libsdl-org/SDL_image/releases/download/release-3.2.4/SDL3_image-devel-3.2.4-VC.zip",
            "include": ["SDL3_image-3.2.4/include"],
            "libpath": ["SDL3_image-3.2.4/lib/x64"],
            "libname": ["SDL3_image.lib"],
            "dll": ["SDL3_image-3.2.4/lib/x64/SDL3_image.dll"]
        },
        "mingw": { 
            "url": "https://github.com/libsdl-org/SDL_image/releases/download/release-3.2.4/SDL3_image-devel-3.2.4-mingw.zip",
            "include": ["SDL3_image-3.2.4/x86_64-w64-mingw32/include"],
            "libpath": ["SDL3_image-3.2.4/x86_64-w64-mingw32/lib"],
            "libname": ["-lSDL3_image"],
            "dll": ["SDL3_image-3.2.4/x86_64-w64-mingw32/bin/SDL3_image.dll"]
        }
    },
    "SDL3_ttf": {
        "msvc": {
            "url": "https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-devel-3.2.2-VC.zip" ,
            "include": ["SDL3_ttf-3.2.2/include"],
            "libpath": ["SDL3_ttf-3.2.2/lib/x64"],
            "libname": ["SDL3_ttf.lib"],
            "dll": ["SDL3_ttf-3.2.2/lib/x64/SDL3_ttf.dll"]

        },
        "mingw": {
            "url": "https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-devel-3.2.2-mingw.zip",
            "include": ["SDL3_ttf-3.2.2/x86_64-w64-mingw32/include"],
            "libpath": ["SDL3_ttf-3.2.2/x86_64-w64-mingw32/lib"],
            "libname": ["-lSDL3_ttf"],
            "dll": ["SDL3_ttf-3.2.2/x86_64-w64-mingw32/bin/SDL3_ttf.dll"]
        }
    }
}


def find_package(package: str) -> Package:
    if package not in KNOWN_PACKAGES:
        raise RuntimeError(f"Unkown package '{package}'")
    if BACKEND.name not in KNOWN_PACKAGES[package]:
        raise RuntimeError(f"Package '{package}' not supported for {BACKEND.name}")

    libdir = pathlib.Path(WORKDIR) / 'lib'
    libdir = libdir.relative_to(WORKDIR)
    if not libdir.exists():
        libdir.mkdir()

    pkg = KNOWN_PACKAGES[package][BACKEND.name]
    pkg_path = libdir / (package + f"-{BACKEND.name}")
    if not pkg_path.exists():
        from urllib.request import urlretrieve
        print(f"Downloading {pkg['url']}")
        path, msg = urlretrieve(pkg['url'], filename=(libdir / package).with_suffix(".zip"))

        print(f"Extracting {path}")
        import zipfile
        with zipfile.ZipFile(path, 'r') as zfile:
            zfile.extractall(pkg_path)

        if "hook" in pkg:
            pkg['hook'](pkg_path, pkg)

    includes = pkg['include']
    libpath = pkg['libpath']
    libname = pkg['libname']
    dll = pkg['dll']

    i_flags = ' '.join(BACKEND.include(str(pkg_path / i)) for i in includes)
    l_flags = ' '.join(BACKEND.libpath(str(pkg_path / l)) for l in libpath)
    lib_flags = ' '.join(libname)
    link_flags = ' '.join([l_flags, lib_flags])
    if "sources" in pkg:
        sources = [str(pkg_path / s) for s in pkg['sources']]
    else:
        sources = []
    dlls = [str(pkg_path / d) for d in dll]
    return Package(i_flags, link_flags, dlls, sources)
