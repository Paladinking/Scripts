all: \
	bin\pathc.exe \
	bin\parse-template.exe \
	bin\path-add.exe \
	bin\envir.exe \
	bin\path-edit.exe \
	bin\path-select.exe \
	bin\inject.exe \
	bin\list-dir.exe \
	bin\autocmp.dll \
	bin\test.exe \
	bin\symbol-dump.exe \
	bin\symbol-scrape.exe \
	bin\autocmp.json \
	bin\err.exe \
	bin\2to3.bat \
	bin\cal.bat \
	bin\ports.bat \
	bin\short.bat \
	bin\wget.bat \
	bin\xkcd.bat \
	bin\xkcd-keywords.txt \
	bin\xkcd-titles.txt \
	bin\vcvarsall.ps1 \
	bin\cmdrc.bat \
	bin\password.bat \
	bin\vcvarsall.bat

build-dbg:
	-@ if NOT EXIST "build-dbg" mkdir "build-dbg"
bin:
	-@ if NOT EXIST "bin" mkdir "bin"
script:
	-@ if NOT EXIST "script" mkdir "script"

build-dbg\pathc.obj: src\pathc.c src\args.h src\path_utils.h src\dynamic_string.h src\mem.h src\printf.h build-dbg
	cl.exe /c /Fo:build-dbg\pathc.obj /GS- /GL /O1 /favor:AMD64 /nologo src\pathc.c
build-dbg\path_utils.obj: src\path_utils.c src\path_utils.h src\dynamic_string.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\path_utils.obj /GS- /GL /O1 /favor:AMD64 /nologo src\path_utils.c
build-dbg\args.obj: src\args.c src\args.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\args.obj /GS- /GL /O1 /favor:AMD64 /nologo src\args.c
build-dbg\mem.obj: src\mem.c src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\mem.obj /GS- /GL /O1 /favor:AMD64 /nologo src\mem.c
build-dbg\dynamic_string.obj: src\dynamic_string.c src\dynamic_string.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\dynamic_string.obj /GS- /GL /O1 /favor:AMD64 /nologo src\dynamic_string.c
build-dbg\printf.obj: src\printf.c src\printf.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\printf.obj /GS- /GL /O1 /favor:AMD64 /nologo src\printf.c
build-dbg\parse-template.obj: src\parse-template.c src\args.h src\mem.h src\dynamic_string.h build-dbg
	cl.exe /c /Fo:build-dbg\parse-template.obj /GS- /GL /O1 /favor:AMD64 /nologo src\parse-template.c
build-dbg\path-add.obj: src\path-add.c src\args.h src\mem.h src\dynamic_string.h src\path_utils.h src\printf.h build-dbg
	cl.exe /c /Fo:build-dbg\path-add.obj /GS- /GL /O1 /favor:AMD64 /nologo src\path-add.c
build-dbg\envir.obj: src\envir.c src\printf.h src\args.h src\path_utils.h src\dynamic_string.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\envir.obj /GS- /GL /O1 /favor:AMD64 /nologo src\envir.c
build-dbg\path-edit.obj: src\path-edit.c src\cli.h src\dynamic_string.h src\mem.h src\printf.h build-dbg
	cl.exe /c /Fo:build-dbg\path-edit.obj /GS- /GL /O1 /favor:AMD64 /nologo src\path-edit.c
build-dbg\cli.obj: src\cli.c src\cli.h src\dynamic_string.h src\mem.h src\printf.h src\unicode_width.h build-dbg
	cl.exe /c /Fo:build-dbg\cli.obj /GS- /GL /O1 /favor:AMD64 /nologo src\cli.c
build-dbg\unicode_width.obj: src\unicode_width.c src\unicode_width.h build-dbg
	cl.exe /c /Fo:build-dbg\unicode_width.obj /GS- /GL /O1 /favor:AMD64 /nologo src\unicode_width.c
build-dbg\path-select.obj: src\path-select.c src\args.h src\printf.h src\mem.h src\path_utils.h src\dynamic_string.h build-dbg
	cl.exe /c /Fo:build-dbg\path-select.obj /GS- /GL /O1 /favor:AMD64 /nologo src\path-select.c
build-dbg\inject.obj: src\inject.c src\printf.h src\args.h src\glob.h src\dynamic_string.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\inject.obj /GS- /GL /O1 /favor:AMD64 /nologo src\inject.c
build-dbg\glob.obj: src\glob.c src\glob.h src\dynamic_string.h src\mem.h src\args.h build-dbg
	cl.exe /c /Fo:build-dbg\glob.obj /GS- /GL /O1 /favor:AMD64 /nologo src\glob.c
build-dbg\list-dir.obj: src\list-dir.c src\args.h src\glob.h src\dynamic_string.h src\mem.h src\perm.h src\printf.h src\unicode_width.h build-dbg
	cl.exe /c /Fo:build-dbg\list-dir.obj /GS- /GL /O1 /favor:AMD64 /nologo src\list-dir.c
build-dbg\perm.obj: src\perm.c src\perm.h src\dynamic_string.h src\mem.h src\printf.h build-dbg
	cl.exe /c /Fo:build-dbg\perm.obj /GS- /GL /O1 /favor:AMD64 /nologo src\perm.c
build-dbg\whashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\whashmap.obj /GS- /GL /O1 /favor:AMD64 /nologo /DHASHMAP_WIDE /DHASHMAP_CASE_INSENSITIVE src\hashmap.c
build-dbg\lhashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\lhashmap.obj /GS- /GL /O1 /favor:AMD64 /nologo /DHASHMAP_LINKED src\hashmap.c
build-dbg\hashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\hashmap.obj /GS- /GL /O1 /favor:AMD64 /nologo src\hashmap.c
build-dbg\autocmp.obj: src\autocmp.c src\printf.h src\dynamic_string.h src\mem.h src\json.h src\linkedhashmap.h src\hashmap.h src\match_node.h src\glob.h src\whashmap.h src\path_utils.h src\subprocess.h src\cli.h build-dbg
	cl.exe /c /Fo:build-dbg\autocmp.obj /GS- /GL /O1 /favor:AMD64 /nologo src\autocmp.c
build-dbg\match_node.obj: src\match_node.c src\match_node.h src\glob.h src\dynamic_string.h src\mem.h src\whashmap.h src\hashmap.h src\subprocess.h src\args.h src\printf.h build-dbg
	cl.exe /c /Fo:build-dbg\match_node.obj /GS- /GL /O1 /favor:AMD64 /nologo src\match_node.c
build-dbg\subprocess.obj: src\subprocess.c src\subprocess.h src\dynamic_string.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\subprocess.obj /GS- /GL /O1 /favor:AMD64 /nologo src\subprocess.c
build-dbg\json.obj: src\json.c src\json.h src\linkedhashmap.h src\hashmap.h src\mem.h src\dynamic_string.h build-dbg
	cl.exe /c /Fo:build-dbg\json.obj /GS- /GL /O1 /favor:AMD64 /nologo src\json.c
build-dbg\test.obj: src\test.c src\args.h src\glob.h src\dynamic_string.h src\mem.h src\printf.h build-dbg
	cl.exe /c /Fo:build-dbg\test.obj /GS- /GL /O1 /favor:AMD64 /nologo src\test.c
build-dbg\symbol-dump.obj: src\symbol-dump.c src\coff.h src\printf.h src\args.h src\mem.h build-dbg
	cl.exe /c /Fo:build-dbg\symbol-dump.obj /GS- /GL /O1 /favor:AMD64 /nologo src\symbol-dump.c
build-dbg\coff.obj: src\coff.c src\mem.h src\coff.h src\args.h build-dbg
	cl.exe /c /Fo:build-dbg\coff.obj /GS- /GL /O1 /favor:AMD64 /nologo src\coff.c
build-dbg\symbol-scrape.obj: src\symbol-scrape.c src\args.h src\coff.h src\glob.h src\dynamic_string.h src\mem.h src\hashmap.h src\path_utils.h src\printf.h src\whashmap.h build-dbg
	cl.exe /c /Fo:build-dbg\symbol-scrape.obj /GS- /GL /O1 /favor:AMD64 /nologo src\symbol-scrape.c

bin\pathc.exe: build-dbg\pathc.obj build-dbg\path_utils.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib bin
	link /OUT:bin\pathc.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\pathc.obj build-dbg\path_utils.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib
bin\parse-template.exe: build-dbg\parse-template.obj build-dbg\args.obj build-dbg\dynamic_string.obj build-dbg\mem.obj build-dbg\ntutils.lib bin
	link /OUT:bin\parse-template.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\parse-template.obj build-dbg\args.obj build-dbg\dynamic_string.obj build-dbg\mem.obj build-dbg\ntutils.lib
bin\path-add.exe: build-dbg\path-add.obj build-dbg\path_utils.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib bin
	link /OUT:bin\path-add.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\path-add.obj build-dbg\path_utils.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib
bin\envir.exe: build-dbg\envir.obj build-dbg\path_utils.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib bin
	link /OUT:bin\envir.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\envir.obj build-dbg\path_utils.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib
bin\path-edit.exe: build-dbg\path-edit.obj build-dbg\cli.obj build-dbg\unicode_width.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib bin
	link /OUT:bin\path-edit.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\path-edit.obj build-dbg\cli.obj build-dbg\unicode_width.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib
bin\path-select.exe: build-dbg\path-select.obj build-dbg\path_utils.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib build-dbg\kernelbase.lib bin
	link /OUT:bin\path-select.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\path-select.obj build-dbg\path_utils.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib build-dbg\kernelbase.lib
bin\inject.exe: build-dbg\inject.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\glob.obj build-dbg\ntutils.lib bin
	link /OUT:bin\inject.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\inject.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\glob.obj build-dbg\ntutils.lib
bin\list-dir.exe: build-dbg\list-dir.obj build-dbg\args.obj build-dbg\printf.obj build-dbg\perm.obj build-dbg\glob.obj build-dbg\dynamic_string.obj build-dbg\mem.obj build-dbg\unicode_width.obj build-dbg\ntutils.lib bin
	link /OUT:bin\list-dir.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main Advapi32.lib build-dbg\list-dir.obj build-dbg\args.obj build-dbg\printf.obj build-dbg\perm.obj build-dbg\glob.obj build-dbg\dynamic_string.obj build-dbg\mem.obj build-dbg\unicode_width.obj build-dbg\ntutils.lib
bin\autocmp.dll: build-dbg\autocmp.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\match_node.obj build-dbg\subprocess.obj build-dbg\whashmap.obj build-dbg\lhashmap.obj build-dbg\json.obj build-dbg\cli.obj build-dbg\glob.obj build-dbg\path_utils.obj build-dbg\unicode_width.obj build-dbg\ntutils.lib bin
	link /OUT:bin\autocmp.dll /DLL kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:DLLMain build-dbg\autocmp.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\match_node.obj build-dbg\subprocess.obj build-dbg\whashmap.obj build-dbg\lhashmap.obj build-dbg\json.obj build-dbg\cli.obj build-dbg\glob.obj build-dbg\path_utils.obj build-dbg\unicode_width.obj build-dbg\ntutils.lib
bin\test.exe: build-dbg\test.obj build-dbg\match_node.obj build-dbg\glob.obj build-dbg\cli.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\json.obj build-dbg\subprocess.obj build-dbg\path_utils.obj build-dbg\unicode_width.obj build-dbg\whashmap.obj build-dbg\lhashmap.obj build-dbg\ntutils.lib bin
	link /OUT:bin\test.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\test.obj build-dbg\match_node.obj build-dbg\glob.obj build-dbg\cli.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\json.obj build-dbg\subprocess.obj build-dbg\path_utils.obj build-dbg\unicode_width.obj build-dbg\whashmap.obj build-dbg\lhashmap.obj build-dbg\ntutils.lib
bin\symbol-dump.exe: build-dbg\symbol-dump.obj build-dbg\coff.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib bin
	link /OUT:bin\symbol-dump.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\symbol-dump.obj build-dbg\coff.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib
bin\symbol-scrape.exe: build-dbg\symbol-scrape.obj build-dbg\path_utils.obj build-dbg\glob.obj build-dbg\coff.obj build-dbg\whashmap.obj build-dbg\hashmap.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib bin
	link /OUT:bin\symbol-scrape.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build-dbg\symbol-scrape.obj build-dbg\path_utils.obj build-dbg\glob.obj build-dbg\coff.obj build-dbg\whashmap.obj build-dbg\hashmap.obj build-dbg\args.obj build-dbg\mem.obj build-dbg\dynamic_string.obj build-dbg\printf.obj build-dbg\ntutils.lib

build-dbg\ntutils.lib: build-dbg
	lib /MACHINE:X64 /DEF /OUT:build-dbg\ntutils.lib /NAME:ntdll.dll /EXPORT:memcpy=memcpy /EXPORT:strlen=strlen /EXPORT:memmove=memmove /EXPORT:_wsplitpath_s=_wsplitpath_s /EXPORT:wcslen=wcslen /EXPORT:_wmakepath_s=_wmakepath_s /EXPORT:strchr=strchr /EXPORT:_stricmp=_stricmp /EXPORT:towlower=towlower /EXPORT:_wcsicmp=_wcsicmp /EXPORT:_snwprintf_s=_snwprintf_s /EXPORT:_snprintf_s=_snprintf_s /EXPORT:_vscwprintf=_vscwprintf /EXPORT:_vsnprintf=_vsnprintf /EXPORT:_vsnwprintf=_vsnwprintf /EXPORT:_vscprintf=_vscprintf /EXPORT:memset=memset /EXPORT:wcscmp=wcscmp /EXPORT:strcmp=strcmp /EXPORT:_fltused=_fltused /EXPORT:wcschr=wcschr /EXPORT:wcsrchr=wcsrchr /EXPORT:strtol=strtol /EXPORT:wcsncmp=wcsncmp /EXPORT:memcmp=memcmp /EXPORT:tolower=tolower /EXPORT:strncmp=strncmp /EXPORT:strnlen=strnlen /EXPORT:wcsstr=wcsstr /EXPORT:_snprintf=_snprintf
build-dbg\kernelbase.lib: build-dbg
	lib /MACHINE:X64 /DEF /OUT:build-dbg\kernelbase.lib /NAME:kernelbase.dll /EXPORT:PathMatchSpecW=PathMatchSpecW
bin\autocmp.json: autocmp.json bin
	type autocmp.json > bin\autocmp.json
bin\err.exe: script\err.exe bin
	type script\err.exe > bin\err.exe
bin\2to3.bat: script\2to3.bat bin
	type script\2to3.bat > bin\2to3.bat
bin\cal.bat: script\cal.bat bin
	type script\cal.bat > bin\cal.bat
bin\ports.bat: script\ports.bat bin
	type script\ports.bat > bin\ports.bat
bin\short.bat: script\short.bat bin
	type script\short.bat > bin\short.bat
bin\wget.bat: script\wget.bat bin
	type script\wget.bat > bin\wget.bat
bin\xkcd.bat: script\xkcd.bat bin
	type script\xkcd.bat > bin\xkcd.bat
bin\xkcd-keywords.txt: script\xkcd-keywords.txt bin
	type script\xkcd-keywords.txt > bin\xkcd-keywords.txt
bin\xkcd-titles.txt: script\xkcd-titles.txt bin
	type script\xkcd-titles.txt > bin\xkcd-titles.txt
bin\vcvarsall.ps1: script\vcvarsall.ps1 bin
	type script\vcvarsall.ps1 > bin\vcvarsall.ps1
script\translations: script
	@echo File script\translations missing
	@echo The following keys are needed: NPP_PATH, PASS_PATH, GIT_PATH, and VCVARS_PATH
bin\cmdrc.bat: script\cmdrc.bat.template bin\parse-template.exe script\translations bin
	bin\parse-template.exe script\cmdrc.bat.template script\translations bin\cmdrc.bat
bin\password.bat: script\password.bat.template bin\parse-template.exe script\translations bin
	bin\parse-template.exe script\password.bat.template script\translations bin\password.bat
bin\vcvarsall.bat: script\vcvarsall.bat.template bin\parse-template.exe script\translations bin
	bin\parse-template.exe script\vcvarsall.bat.template script\translations bin\vcvarsall.bat

clean:
	del /S /Q build-dbg\*
	del /S /Q bin\*
