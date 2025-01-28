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

OBJECTS = dict()
EXECUTABLES = dict()
CUSTOMS = dict()

class Obj:
    def __init__(self, name: str, source: str, cmp_flags="", namespace="") -> None:
        self.name = name
        self.namespace = namespace
        self.source = source
        self.cmp_flags=cmp_flags

def Object(name: str, source: str, cmp_flags=CLFLAGS, namespace="") -> Obj:
    obj = Obj(name, source, cmp_flags, namespace)

    if namespace:
        key = namespace + "\\" + name
        obj.name = key
    else:
        key = name
    if key in OBJECTS:
        other = OBJECTS[key]
        if other.cmp_flags == obj.cmp_flags and other.source == obj.source:
            return other
        raise RuntimeError(f"Duplicate object {key}")
    OBJECTS[key] = obj
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
    def __init__(self, name: str, objs: List[Obj], cmds: List[Cmd], link_flags="", namespace=""):
        self.name = name
        self.objs = objs
        self.cmds = cmds
        self.link_flags = link_flags

def Command(name: str, cmd: str, *depends, directory=BUILD_DIR) -> Cmd:
    if name in CUSTOMS:
        raise RuntimeError(f"Duplicate command {name}")
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
                cmds.append(src)
                continue
            assert isinstance(src, Obj)
            obj = src
        objs.append(obj)
    exe = Exe(name, objs, cmds, link_flags)
    if name in EXECUTABLES:
        raise RuntimeError(f"Duplicate exe {name}")
    EXECUTABLES[name] = exe
    return exe


def generate():
    gcc = shutil.which("gcc.exe")
    if gcc is None:
        raise RuntimeError("Could not find gcc")

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

    print(f"{BUILD_DIR}:\n\t-@ if NOT EXIST \"{BUILD_DIR}\" mkdir \"{BUILD_DIR}\"")
    if BIN_DIR != BUILD_DIR:
        print(f"{BIN_DIR}:\n\t-@ if NOT EXIST \"{BIN_DIR}\" mkdir \"{BIN_DIR}\"")
    for key, obj in OBJECTS.items():
        if obj.namespace:
            d = f"{BUILD_DIR}\\{obj.namespace}"
            print(f"{d}: {BUILD_DIR}\n\t-@ if NOT EXIST \"{d}\" mkdir \"{d}\"")
    print()

    for key, obj in OBJECTS.items():
        res = subprocess.run(["gcc", "-MM", "-MT", f"{BUILD_DIR}\\{key}", f"{obj.source}"], stdout=subprocess.PIPE)
        if res.returncode != 0:
            raise RuntimeError("Failed generating headers")
        if obj.namespace:
            bld_dir = f"{BUILD_DIR}\\{obj.namespace}"
        else:
            bld_dir = BUILD_DIR
        row = res.stdout.decode("utf-8").strip().replace(" \\\r\n", "") + f" {bld_dir}"
        print(row)
        print(f"\tcl /c /Fo:{BUILD_DIR}\\{key} {obj.cmp_flags} {obj.source}")
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

    arg_src = ["src\\args.c", "src\\mem.c", "src\\dynamic_string.c", "src\\printf.c"]
    ntdll = export_lib("ntutils.lib", "ntdll.dll", ntsymbols)
    kernelbase = export_lib("kernelbase.lib", "kernelbase.dll", kernelbasesymbols)

    Executable("pathc.exe", "src\\pathc.c", "src\\path_utils.c", *arg_src, ntdll)
    Executable("parse-template.exe", "src\\parse-template.c", "src\\args.c",
               "src\\dynamic_string.c", "src\\mem.c", ntdll)
    Executable("path-add.exe", "src\\path-add.c", "src\\path_utils.c", *arg_src, ntdll)
    Executable("envir.exe", "src\\envir.c", "src\\path_utils.c", *arg_src, ntdll)

    Executable("path-edit.exe", "src\\path-edit.c", "src\\cli.c", "src\\unicode_width.c", *arg_src, ntdll)
    Executable("path-select.exe", "src\\path-select.c", "src\\path_utils.c", *arg_src, ntdll, kernelbase)
    Executable("inject.exe", "src\\inject.c", "src\\printf.c", "src\\mem.c", "src\\glob.c", "src\\dynamic_string.c", ntdll)
    
    whashmap = Object("whashmap.obj", "src\\hashmap.c", cmp_flags=CLFLAGS + " /DHASHMAP_WIDE /DHASHMAP_CASE_INSENSITIVE")
    hashmap = Object("hashmap.obj", "src\\hashmap.c", cmp_flags=CLFLAGS + " /DHASHMAP_LINKED")

    Executable("autocmp.dll", "src\\autocmp.c", *arg_src, "src\\match_node.c", "src\\subprocess.c", 
               whashmap, hashmap, "src\\json.c", "src\\glob.c", ntdll, link_flags=DLLFLAGS)
    Executable("test.exe", "src\\test.c", "src\\match_node.c", "src\\glob.c", *arg_src,
               "src\\json.c", "src\\subprocess.c", whashmap, hashmap, ntdll)
    CopyToBin("autocmp.json", "script\\err.exe", "script\\2to3.bat", "script\\cal.bat",
              "script\\ports.bat", "script\\short.bat", "script\\wget.bat", "script\\xkcd.bat",
              "script\\xkcd-keywords.txt", "script\\xkcd-titles.txt", "script\\vcvarsall.ps1")
    Command("translations", "@echo File script\\translations missing\n\t@echo The following keys are needed: NPP_PATH, PASS_PATH, GIT_PATH, and VCVARS_PATH", directory="script")
    translate("cmdrc.bat", "password.bat", "vcvarsall.bat")


    generate()
    print("\ninstall: all\n\tcopy bin\\* ..")


if __name__ == "__main__":
    main()
