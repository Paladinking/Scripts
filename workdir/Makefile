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
	bin\defer.exe \
	bin\find-file.exe \
	bin\type-file.exe \
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

build:
	-@ if NOT EXIST "build" mkdir "build"
bin:
	-@ if NOT EXIST "bin" mkdir "bin"
script:
	-@ if NOT EXIST "script" mkdir "script"

build\pathc.obj: src\pathc.c src\args.h src\path_utils.h src\dynamic_string.h src\mem.h src\printf.h build
	cl.exe /c /Fo:build\pathc.obj /GS- /GL /O1 /favor:AMD64 /nologo src\pathc.c
build\path_utils.obj: src\path_utils.c src\path_utils.h src\dynamic_string.h src\mem.h build
	cl.exe /c /Fo:build\path_utils.obj /GS- /GL /O1 /favor:AMD64 /nologo src\path_utils.c
build\args.obj: src\args.c src\args.h src\mem.h build
	cl.exe /c /Fo:build\args.obj /GS- /GL /O1 /favor:AMD64 /nologo src\args.c
build\mem.obj: src\mem.c src\mem.h build
	cl.exe /c /Fo:build\mem.obj /GS- /GL /O1 /favor:AMD64 /nologo src\mem.c
build\dynamic_string.obj: src\dynamic_string.c src\dynamic_string.h src\mem.h build
	cl.exe /c /Fo:build\dynamic_string.obj /GS- /GL /O1 /favor:AMD64 /nologo src\dynamic_string.c
build\printf.obj: src\printf.c src\printf.h src\mem.h build
	cl.exe /c /Fo:build\printf.obj /GS- /GL /O1 /favor:AMD64 /nologo src\printf.c
build\parse-template.obj: src\parse-template.c src\args.h src\mem.h src\dynamic_string.h build
	cl.exe /c /Fo:build\parse-template.obj /GS- /GL /O1 /favor:AMD64 /nologo src\parse-template.c
build\path-add.obj: src\path-add.c src\args.h src\mem.h src\dynamic_string.h src\path_utils.h src\printf.h build
	cl.exe /c /Fo:build\path-add.obj /GS- /GL /O1 /favor:AMD64 /nologo src\path-add.c
build\envir.obj: src\envir.c src\printf.h src\args.h src\path_utils.h src\dynamic_string.h src\mem.h build
	cl.exe /c /Fo:build\envir.obj /GS- /GL /O1 /favor:AMD64 /nologo src\envir.c
build\path-edit.obj: src\path-edit.c src\cli.h src\dynamic_string.h src\mem.h src\printf.h build
	cl.exe /c /Fo:build\path-edit.obj /GS- /GL /O1 /favor:AMD64 /nologo src\path-edit.c
build\cli.obj: src\cli.c src\cli.h src\dynamic_string.h src\mem.h src\printf.h src\unicode_width.h build
	cl.exe /c /Fo:build\cli.obj /GS- /GL /O1 /favor:AMD64 /nologo src\cli.c
build\unicode_width.obj: src\unicode_width.c src\unicode_width.h build
	cl.exe /c /Fo:build\unicode_width.obj /GS- /GL /O1 /favor:AMD64 /nologo src\unicode_width.c
build\path-select.obj: src\path-select.c src\args.h src\printf.h src\mem.h src\path_utils.h src\dynamic_string.h build
	cl.exe /c /Fo:build\path-select.obj /GS- /GL /O1 /favor:AMD64 /nologo src\path-select.c
build\inject.obj: src\inject.c src\printf.h src\args.h src\glob.h src\dynamic_string.h src\mem.h build
	cl.exe /c /Fo:build\inject.obj /GS- /GL /O1 /favor:AMD64 /nologo src\inject.c
build\glob.obj: src\glob.c src\glob.h src\dynamic_string.h src\mem.h src\args.h build
	cl.exe /c /Fo:build\glob.obj /GS- /GL /O1 /favor:AMD64 /nologo src\glob.c
build\list-dir.obj: src\list-dir.c src\args.h src\glob.h src\dynamic_string.h src\mem.h src\perm.h src\printf.h src\unicode_width.h build
	cl.exe /c /Fo:build\list-dir.obj /GS- /GL /O1 /favor:AMD64 /nologo src\list-dir.c
build\perm.obj: src\perm.c src\perm.h src\dynamic_string.h src\mem.h src\printf.h build
	cl.exe /c /Fo:build\perm.obj /GS- /GL /O1 /favor:AMD64 /nologo src\perm.c
build\whashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build
	cl.exe /c /Fo:build\whashmap.obj /GS- /GL /O1 /favor:AMD64 /nologo /DHASHMAP_WIDE /DHASHMAP_CASE_INSENSITIVE src\hashmap.c
build\lhashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build
	cl.exe /c /Fo:build\lhashmap.obj /GS- /GL /O1 /favor:AMD64 /nologo /DHASHMAP_LINKED src\hashmap.c
build\hashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build
	cl.exe /c /Fo:build\hashmap.obj /GS- /GL /O1 /favor:AMD64 /nologo src\hashmap.c
build\autocmp.obj: src\autocmp.c src\printf.h src\dynamic_string.h src\mem.h src\json.h src\linkedhashmap.h src\hashmap.h src\match_node.h src\glob.h src\whashmap.h src\path_utils.h src\subprocess.h src\cli.h build
	cl.exe /c /Fo:build\autocmp.obj /GS- /GL /O1 /favor:AMD64 /nologo src\autocmp.c
build\match_node.obj: src\match_node.c src\match_node.h src\glob.h src\dynamic_string.h src\mem.h src\whashmap.h src\hashmap.h src\subprocess.h src\args.h src\printf.h build
	cl.exe /c /Fo:build\match_node.obj /GS- /GL /O1 /favor:AMD64 /nologo src\match_node.c
build\subprocess.obj: src\subprocess.c src\subprocess.h src\dynamic_string.h src\mem.h build
	cl.exe /c /Fo:build\subprocess.obj /GS- /GL /O1 /favor:AMD64 /nologo src\subprocess.c
build\json.obj: src\json.c src\json.h src\linkedhashmap.h src\hashmap.h src\mem.h src\dynamic_string.h build
	cl.exe /c /Fo:build\json.obj /GS- /GL /O1 /favor:AMD64 /nologo src\json.c
build\test.obj: src\test.c src\args.h src\regex.h src\glob.h src\dynamic_string.h src\mem.h src\printf.h build
	cl.exe /c /Fo:build\test.obj /GS- /GL /O1 /favor:AMD64 /nologo src\test.c
build\regex.obj: src\regex.c src\regex.h src\dynamic_string.h src\mem.h build
	cl.exe /c /Fo:build\regex.obj /GS- /GL /O1 /favor:AMD64 /nologo src\regex.c
build\symbol-dump.obj: src\symbol-dump.c src\coff.h src\printf.h src\args.h src\mem.h build
	cl.exe /c /Fo:build\symbol-dump.obj /GS- /GL /O1 /favor:AMD64 /nologo src\symbol-dump.c
build\coff.obj: src\coff.c src\mem.h src\coff.h src\args.h build
	cl.exe /c /Fo:build\coff.obj /GS- /GL /O1 /favor:AMD64 /nologo src\coff.c
build\symbol-scrape.obj: src\symbol-scrape.c src\args.h src\coff.h src\glob.h src\dynamic_string.h src\mem.h src\hashmap.h src\path_utils.h src\printf.h src\whashmap.h build
	cl.exe /c /Fo:build\symbol-scrape.obj /GS- /GL /O1 /favor:AMD64 /nologo src\symbol-scrape.c
build\defer.obj: src\defer.c src\dynamic_string.h src\mem.h src\args.h src\glob.h src\printf.h build
	cl.exe /c /Fo:build\defer.obj /GS- /GL /O1 /favor:AMD64 /nologo src\defer.c
build\find-file.obj: src\find-file.c src\args.h src\mem.h src\glob.h src\dynamic_string.h src\printf.h build
	cl.exe /c /Fo:build\find-file.obj /GS- /GL /O1 /favor:AMD64 /nologo src\find-file.c
build\type-file.obj: src\type-file.c src\args.h src\glob.h src\dynamic_string.h src\mem.h src\printf.h build
	cl.exe /c /Fo:build\type-file.obj /GS- /GL /O1 /favor:AMD64 /nologo src\type-file.c

bin\pathc.exe: build\pathc.obj build\path_utils.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\pathc.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\pathc.obj build\path_utils.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib
bin\parse-template.exe: build\parse-template.obj build\args.obj build\dynamic_string.obj build\mem.obj build\ntutils.lib bin
	link /OUT:bin\parse-template.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\parse-template.obj build\args.obj build\dynamic_string.obj build\mem.obj build\ntutils.lib
bin\path-add.exe: build\path-add.obj build\path_utils.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\path-add.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\path-add.obj build\path_utils.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib
bin\envir.exe: build\envir.obj build\path_utils.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\envir.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\envir.obj build\path_utils.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib
bin\path-edit.exe: build\path-edit.obj build\cli.obj build\unicode_width.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\path-edit.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\path-edit.obj build\cli.obj build\unicode_width.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib
bin\path-select.exe: build\path-select.obj build\path_utils.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib build\kernelbase.lib bin
	link /OUT:bin\path-select.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\path-select.obj build\path_utils.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib build\kernelbase.lib
bin\inject.exe: build\inject.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\glob.obj build\ntutils.lib bin
	link /OUT:bin\inject.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\inject.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\glob.obj build\ntutils.lib
bin\list-dir.exe: build\list-dir.obj build\args.obj build\printf.obj build\perm.obj build\glob.obj build\dynamic_string.obj build\mem.obj build\unicode_width.obj build\ntutils.lib bin
	link /OUT:bin\list-dir.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main Advapi32.lib build\list-dir.obj build\args.obj build\printf.obj build\perm.obj build\glob.obj build\dynamic_string.obj build\mem.obj build\unicode_width.obj build\ntutils.lib
bin\autocmp.dll: build\autocmp.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\match_node.obj build\subprocess.obj build\whashmap.obj build\lhashmap.obj build\json.obj build\cli.obj build\glob.obj build\path_utils.obj build\unicode_width.obj build\ntutils.lib bin
	link /OUT:bin\autocmp.dll /DLL kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:DLLMain build\autocmp.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\match_node.obj build\subprocess.obj build\whashmap.obj build\lhashmap.obj build\json.obj build\cli.obj build\glob.obj build\path_utils.obj build\unicode_width.obj build\ntutils.lib
bin\test.exe: build\test.obj build\glob.obj build\regex.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\test.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\test.obj build\glob.obj build\regex.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib
bin\symbol-dump.exe: build\symbol-dump.obj build\coff.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\symbol-dump.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\symbol-dump.obj build\coff.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib
bin\symbol-scrape.exe: build\symbol-scrape.obj build\path_utils.obj build\glob.obj build\coff.obj build\whashmap.obj build\hashmap.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\symbol-scrape.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\symbol-scrape.obj build\path_utils.obj build\glob.obj build\coff.obj build\whashmap.obj build\hashmap.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib
bin\defer.exe: build\defer.obj build\glob.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\defer.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\defer.obj build\glob.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib
bin\find-file.exe: build\find-file.obj build\glob.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\find-file.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\find-file.obj build\glob.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib
bin\type-file.exe: build\type-file.obj build\glob.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib bin
	link /OUT:bin\type-file.exe kernel32.lib chkstk.obj /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main build\type-file.obj build\glob.obj build\args.obj build\mem.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib

build\ntutils.lib: build
	lib /NOLOGO /MACHINE:X64 /DEF /OUT:build\ntutils.lib /NAME:ntdll.dll /EXPORT:memcpy=memcpy /EXPORT:strlen=strlen /EXPORT:memmove=memmove /EXPORT:_wsplitpath_s=_wsplitpath_s /EXPORT:wcslen=wcslen /EXPORT:_wmakepath_s=_wmakepath_s /EXPORT:strchr=strchr /EXPORT:_stricmp=_stricmp /EXPORT:towlower=towlower /EXPORT:_wcsicmp=_wcsicmp /EXPORT:_snwprintf_s=_snwprintf_s /EXPORT:_snprintf_s=_snprintf_s /EXPORT:_vscwprintf=_vscwprintf /EXPORT:_vsnprintf=_vsnprintf /EXPORT:_vsnwprintf=_vsnwprintf /EXPORT:_vscprintf=_vscprintf /EXPORT:memset=memset /EXPORT:wcscmp=wcscmp /EXPORT:strcmp=strcmp /EXPORT:_fltused=_fltused /EXPORT:wcschr=wcschr /EXPORT:wcsrchr=wcsrchr /EXPORT:strtol=strtol /EXPORT:wcsncmp=wcsncmp /EXPORT:memcmp=memcmp /EXPORT:tolower=tolower /EXPORT:strncmp=strncmp /EXPORT:strnlen=strnlen /EXPORT:wcsstr=wcsstr /EXPORT:_snprintf=_snprintf
build\kernelbase.lib: build
	lib /NOLOGO /MACHINE:X64 /DEF /OUT:build\kernelbase.lib /NAME:kernelbase.dll /EXPORT:PathMatchSpecW=PathMatchSpecW
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
	del /S /Q build\*
	del /S /Q bin\*
