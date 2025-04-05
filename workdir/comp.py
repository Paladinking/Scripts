from comp_backend import *
import pathlib

CLFLAGS="/GS- /GL /O1 /favor:AMD64 /nologo"
LINKFLAGS="kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main"
DLLFLAGS="kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:DLLMain"

BUILD_DIR = "build"
BIN_DIR = "bin"


CLFLAGS_DBG = "-g -Og"
LINKFLAGS_DBG = "-g"

BUILD_DIR_DBG = "build-gcc"
BIN_DIR_DBG = "bin-gcc"

BUILD_DIR_ZIG = "build-zig"
BIN_DIR_ZIG = "bin-zig"


WORKDIR = pathlib.Path(__file__).parent.resolve()

def translate(*src: str) -> List[Cmd]:
    res = []
    for file in src:
        template = f"script\\{file}.template"
        exe = f"{bin_dir()}\\parse-template.exe"
        cmd = Command(file, f"{exe} {template} script\\translations {bin_dir()}\\{file}",
                      template, exe, "script\\translations", directory=bin_dir())
        res.append(cmd)
    return res


add_backend("Msvc", BUILD_DIR, BIN_DIR, WORKDIR, CLFLAGS, LINKFLAGS)
add_backend("Mingw", BUILD_DIR_DBG, BIN_DIR_DBG, WORKDIR, CLFLAGS_DBG, LINKFLAGS_DBG)
add_backend("Zigcc", BUILD_DIR_ZIG, BIN_DIR_ZIG, WORKDIR, CLFLAGS_DBG, LINKFLAGS_DBG)

set_backend("Msvc")

if not backend().msvc:
    DLLFLAGS = None

def main():
    ntsymbols = ["memcpy", "strlen", "memmove", "_wsplitpath_s", "wcslen",
                 "_wmakepath_s", "strchr", "_stricmp", "towlower",
                 "_wcsicmp", "_snwprintf_s", "_snprintf_s", "_vscwprintf", 
                 "_vsnprintf", "_vsnwprintf", "_vscprintf", "memset", 
                 "wcscmp", "strcmp", "_fltused", "wcschr", "wcsrchr", 
                 "strtol", "wcsncmp", "memcmp", "tolower", "strncmp",
                 "strnlen", "wcsstr", "_snprintf"]
    kernelbasesymbols = ["PathMatchSpecW"]

    arg_src = ["src/args.c", "src/mem.c", "src/dynamic_string.c", "src/printf.c"]
    ntdll = ImportLib("ntutils.lib", "ntdll.dll", ntsymbols)
    kernelbase = ImportLib("kernelbase.lib", "kernelbase.dll", kernelbasesymbols)

    #embed = Executable("embed.exe", "tools/embed.c", cmp_flags="", link_flags="")

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
               "src/perm.c", "src/glob.c", "src/dynamic_string.c", "src/mem.c",
               "src/unicode_width.c", ntdll, 
               extra_link_flags="Advapi32.lib" if backend().msvc else "-ladvapi32")
    
    whashmap = Object("whashmap.obj", "src/hashmap.c", 
                      defines=['HASHMAP_WIDE', 'HASHMAP_CASE_INSENSITIVE'])
    lhashmap = Object("lhashmap.obj", "src/hashmap.c", defines=['HASHMAP_LINKED'])
    hashmap = Object("hashmap.obj", "src/hashmap.c")

    Executable("autocmp.dll", "src/autocmp.c", *arg_src, "src/match_node.c",
               "src/subprocess.c", whashmap, lhashmap, "src/json.c", 
               "src/cli.c", "src/glob.c", "src/path_utils.c", ntdll,
               "src/unicode_width.c",
               link_flags=DLLFLAGS, dll=True)
    Executable("test.exe", "src/test.c", "src/glob.c", "src/regex.c", *arg_src, ntdll)
    Executable("symbol-dump.exe", "src/symbol-dump.c", "src/coff.c", *arg_src, ntdll)
    scrape = Executable("symbol-scrape.exe", "src/symbol-scrape.c", "src/path_utils.c",
               "src/glob.c", "src/coff.c", whashmap, hashmap, *arg_src, ntdll)

    #index = [
    #    Command(f"index_dll.bin", f"{scrape.product} --index-only --dll",
    #            scrape, directory=f"{bin_dir()}/index"),
    #    Command(f"index_lib.bin", f"{scrape.product} --index-only --lib",
    #            scrape, directory=f"{bin_dir()}/index"),
    #    Command(f"index_obj.bin", f"{scrape.product} --index-only --obj",
    #            scrape, directory=f"{bin_dir()}/index"),
    #]
    #index_files = f"{bin_dir()}\\index\\index_o.bin " + \
    #              " ".join(cmd.product for cmd in index)
    #embed_index = Command(f"index.h", 
    #                      f"{embed.product} -o tools\\index.obj -H " + 
    #                      f"tools\\index.h -w {index_files}",
    #                      embed, *index, directory="tools")

    #symbols = Object("symbols.obj", "src/symbol-scrape.c", 
    #                 defines=["EMBEDDED_SYMBOLS"], depends=[embed_index])
    #Executable("symbols.exe", symbols, hashmap, *arg_src, ntdll, 
    #           extra_link_flags="tools\\index.obj")

    Executable("defer.exe", "src/defer.c", "src/glob.c", *arg_src, ntdll)

    Executable("find-file.exe", "src/find-file.c", "src/glob.c", *arg_src, ntdll)
    Executable("type-file.exe", "src/type-file.c", "src/glob.c", *arg_src, ntdll)

    CopyToBin("autocmp.json", "script/err.exe", "script/2to3.bat",
              "script/cal.bat", "script/ports.bat", "script/short.bat",
              "script/wget.bat", "script/xkcd.bat", "script/xkcd-keywords.txt",
              "script/xkcd-titles.txt", "script/vcvarsall.ps1")
    Command("translations", "@echo File script\\translations missing\n\t"
            "@echo The following keys are needed: NPP_PATH, PASS_PATH"
            ", GIT_PATH, and VCVARS_PATH", directory="script")
    translate("cmdrc.bat", "password.bat", "vcvarsall.bat")


    generate()
    #print(f"\ninstall: all\n\tcopy {BIN_DIR}\\* ..")


if __name__ == "__main__":
    main()
