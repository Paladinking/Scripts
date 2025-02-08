from comp_backend import *
import pathlib

CLFLAGS="/GS- /GL /O1 /favor:AMD64 /nologo"
LINKFLAGS="kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main"
DLLFLAGS="kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:DLLMain"

#CLFLAGS = "-g -Og"
#LINKFLAGS = "-g -Og"
#DLLFLAGS = "-g -Og"

BUILD_DIR = "build"
BIN_DIR = "bin"

WORKDIR = pathlib.Path(__file__).parent.resolve()

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


set_backend(BUILD_DIR, BIN_DIR, WORKDIR, CLFLAGS, LINKFLAGS)

def main():
    ntsymbols = ["memcpy", "strlen", "memmove", "_wsplitpath_s", "wcslen",
                 "_wmakepath_s", "strchr", "_stricmp", "towlower",
                 "_wcsicmp", "_snwprintf_s", "_snprintf_s", "_vscwprintf", 
                 "_vsnprintf", "_vsnwprintf", "_vscprintf", "memset", 
                 "wcscmp", "strcmp", "_fltused", "wcschr", "wcsrchr", 
                 "strtol", "wcsncmp", "memcmp", "tolower", "strncmp",
                 "wcsstr", "_snprintf"]
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
    Executable("list-dir.exe", "src/list-dir.c", "src/args.c", "src/printf.c",
               "src/perm.c", "src/glob.c", "src/dynamic_string.c",
               "src/unicode_width.c", ntdll, link_flags=LINKFLAGS + " " + "AdvAPI32.Lib")
    
    whashmap = Object("whashmap.obj", "src/hashmap.c", cmp_flags=CLFLAGS + " " +
                      define('HASHMAP_WIDE') + " " + define('HASHMAP_CASE_INSENSITIVE'))
    hashmap = Object("hashmap.obj", "src/hashmap.c", cmp_flags=CLFLAGS + " " +
                     define("HASHMAP_LINKED"))

    Executable("autocmp.dll", "src/autocmp.c", *arg_src, "src/match_node.c",
               "src/subprocess.c", whashmap, hashmap, "src/json.c", 
               "src/cli.c", "src/glob.c", "src/path_utils.c", ntdll,
               "src/unicode_width.c", link_flags=DLLFLAGS, dll=True)
    Executable("test.exe", "src/test.c", "src/match_node.c", "src/glob.c", 
               "src/cli.c", *arg_src, "src/json.c", "src/subprocess.c", 
               "src/path_utils.c", "src/unicode_width.c",
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
