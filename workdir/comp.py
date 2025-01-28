from typing import Optional, List
import pathlib
import shutil
import subprocess

CLFLAGS="/GS- /GL /O1 /favor:AMD64 /nologo"
LINKFLAGS="kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main"
DLLFLAGS="/DLL kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:DLLMain"
DEFINES=[]

BUILD_DIR = "build"
BIN_DIR = "bin"

DIRECTORIES: dict = dict([(BUILD_DIR, None), (BIN_DIR, None)])
OBJECTS = dict()
EXECUTABLES = dict()
CUSTOMS = dict()

WORKDIR = pathlib.Path(__file__).parent.resolve()

class Obj:
    def __init__(self, name: str, source: str, cmp_flags="", namespace="") -> None:
        self.name = name
        self.namespace = namespace
        self.source = source
        self.cmp_flags=cmp_flags

    def compile(self) -> str:
        return f"cl.exe /c /Fo:{BUILD_DIR}\\{self.name} {self.cmp_flags} {self.source}"

def Object(name: str, source: str, cmp_flags=CLFLAGS, namespace="") -> Obj:
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
    def __init__(self, name: str, objs: List[Obj], cmds: List[Cmd], link_flags=""):
        self.name = name
        self.objs = objs
        self.cmds = cmds
        self.link_flags = link_flags

def Command(name: str, cmd: str, *depends, directory=BUILD_DIR) -> Cmd:
    if name in CUSTOMS:
        raise RuntimeError(f"Duplicate command {name}")
    DIRECTORIES[directory] = None
    command = Cmd(name, cmd, list(depends), directory)
    CUSTOMS[name] = command
    return command

def CopyToBin(*src: str) -> List[Cmd]:
    res = []
    for file in src:
        name = pathlib.PurePath(file).name
        res.append(Command(name, f"type {file} > {BIN_DIR}\\{name}", directory=BIN_DIR))
    return res

def Executable(name: str, *sources, cmp_flags=CLFLAGS, link_flags=LINKFLAGS, namespace="") -> Exe:
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
    exe = Exe(name, objs, cmds, link_flags)
    if name in EXECUTABLES:
        raise RuntimeError(f"Duplicate exe {name}")
    EXECUTABLES[name] = exe
    return exe

def find_headers(obj: Obj) -> list[str]:
    """This is slow..."""
    cmd = f"cl.exe /P /showIncludes /FiNUL {obj.cmp_flags} {obj.source}"
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if res.returncode != 0:
        print(res)
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

def generate():
    cl = shutil.which("cl.exe")
    if cl is None:
        raise RuntimeError("Could not find cl.exe")

    print("all:", end="")
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
        print(f"\t{obj.compile()}")
    print()
    for key, exe in EXECUTABLES.items():
        row = " ".join([f"{BUILD_DIR}\\{obj.name}" for obj in exe.objs] + [f"{cmd.dir}\\{cmd.name}" for cmd in exe.cmds])
        print(f"{BIN_DIR}\\{exe.name}: {row} {BIN_DIR}")
        print(f"\tlink /OUT:{BIN_DIR}\\{exe.name} {exe.link_flags} {row}")
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
        json["command"] = obj.compile()
        json["file"] = str(pathlib.Path(obj.source).resolve())
        res.append(json)
    import json
    print(json.dumps(res, indent=4))

def export_lib(libname: str, dllname: str, symbols: list) -> Cmd:
    exports = " ".join([f"/EXPORT:{s}={s}" for s in symbols])
    cmd = f"lib /MACHINE:X64 /DEF /OUT:{BUILD_DIR}\\{libname} /NAME:{dllname} {exports}"
    return Command(libname, cmd)

def translate(*src: str) -> List[Cmd]:
    res = []
    for file in src:
        template = f"script\\{file}.template"
        exe = f"{BIN_DIR}\\parse-template.exe"
        cmd = Command(file, f"{exe} {template} script\\translations {BIN_DIR}\\{file}",
                      template, exe, "script\\translations", directory=BIN_DIR)
        res.append(cmd)
    return res

def main():
    ntsymbols = ["memcpy", "strlen", "memmove", "_wsplitpath_s", "wcslen",
                 "_wmakepath_s", "strchr", "_stricmp", "towlower",
                 "_wcsicmp", "_snwprintf_s", "_snprintf_s", "_vscwprintf", 
                 "_vsnprintf", "_vsnwprintf", "_vscprintf", "memset", 
                 "wcscmp", "strcmp", "_fltused", "wcschr", "wcsrchr", 
                 "strtol", "wcsncmp", "memcmp", "tolower", "strncmp", 
                 "_snprintf"]
    kernelbasesymbols = ["PathMatchSpecW"]

    arg_src = ["src/args.c", "src/mem.c", "src/dynamic_string.c", "src/printf.c"]
    ntdll = export_lib("ntutils.lib", "ntdll.dll", ntsymbols)
    kernelbase = export_lib("kernelbase.lib", "kernelbase.dll", kernelbasesymbols)

    Executable("pathc.exe", "src/pathc.c", "src/path_utils.c", *arg_src, ntdll)
    Executable("parse-template.exe", "src/parse-template.c", "src/args.c",
               "src/dynamic_string.c", "src/mem.c", ntdll)
    Executable("path-add.exe", "src/path-add.c", "src/path_utils.c", 
               *arg_src, ntdll)
    Executable("envir.exe", "src/envir.c", "src/path_utils.c", *arg_src, ntdll)

    Executable("path-edit.exe", "src/path-edit.c", "src/cli.c", 
               "src/unicode_width.c", *arg_src, ntdll)
    Executable("path-select.exe", "src/path-select.c", "src/path_utils.c",
               *arg_src, ntdll, kernelbase)
    Executable("inject.exe", "src/inject.c", *arg_src, 
               "src/glob.c", ntdll)
    
    whashmap = Object("whashmap.obj", "src/hashmap.c", cmp_flags=CLFLAGS + 
                      " /DHASHMAP_WIDE /DHASHMAP_CASE_INSENSITIVE")
    hashmap = Object("hashmap.obj", "src/hashmap.c", cmp_flags=CLFLAGS + 
                     " /DHASHMAP_LINKED")

    Executable("autocmp.dll", "src/autocmp.c", *arg_src, "src/match_node.c",
               "src/subprocess.c", whashmap, hashmap, "src/json.c", 
               "src/glob.c", ntdll, link_flags=DLLFLAGS)
    Executable("test.exe", "src/test.c", "src/match_node.c", "src/glob.c", 
               *arg_src, "src/json.c", "src/subprocess.c", 
               whashmap, hashmap, ntdll)
    CopyToBin("autocmp.json", "script/err.exe", "script/2to3.bat",
              "script/cal.bat", "script/ports.bat", "script/short.bat",
              "script/wget.bat", "script/xkcd.bat", "script/xkcd-keywords.txt",
              "script/xkcd-titles.txt", "script/vcvarsall.ps1")
    Command("translations", "@echo File script\\translations missing\n\t"
            "@echo The following keys are needed: NPP_PATH, PASS_PATH"
            ", GIT_PATH, and VCVARS_PATH", directory="script")
    translate("cmdrc.bat", "password.bat", "vcvarsall.bat")


    compile_commands()
    #print(f"\ninstall: all\n\tcopy {BIN_DIR}\\* ..")


if __name__ == "__main__":
    main()
