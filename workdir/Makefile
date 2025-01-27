
CLFLAGS=/Fo:build\ /GS- /GL /O1 /favor:AMD64 /nologo
LINKFLAGS=Kernel32.lib /link /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /entry:main
LINKFLAGSW=Kernel32.lib /link /NODEFAULTLIB /SUBSYSTEM:WINDOWS /LTCG /entry:main

all: bin\pathc.exe \
	bin\parse-template.exe \
	bin\path-add.exe \
	bin\envir.exe \
	bin\search.exe \
	bin\picker.exe \
	bin\path-edit.exe \
	bin\path-select.exe \
	bin\inject.exe \
	bin\autocmp.dll \
	bin\2to3.bat \
	bin\cal.bat \
	bin\cleanenv.bat \
	bin\cleanenv.ps1 \
	bin\clip2.bat \
	bin\cmdrc.bat \
	bin\err.exe \
	bin\exp.bat \
	bin\grep.bat \
	bin\ls.bat \
	bin\password.bat \
	bin\pathc.bat \
	bin\ports.bat \
	bin\short.bat \
	bin\unzip.bat \
	bin\vcvarsall.bat \
	bin\vcvarsall.ps1 \
	bin\wget.bat \
	bin\xkcd-keywords.txt \
	bin\xkcd-titles.txt \
	bin\xkcd.bat \
	bin\autocmp.json \
#	bin\path-add.bat \
#	bin\path-add.ps1

build:
	-@ if NOT EXIST "build" mkdir "build"

bin:
	-@ if NOT EXIST "bin" mkdir "bin"
	
build\dynamic_string.obj: src\dynamic_string.c src\dynamic_string.h src\mem.h build
	cl /c $(CLFLAGS) src\dynamic_string.c
build\args.obj: src\args.c src\args.h src\mem.h build
	cl /c $(CLFLAGS) src\args.c
build\printf.obj: src\printf.c src\printf.h src\mem.h build
	cl /c $(CLFLAGS) src\printf.c
build\path_utils.obj: src\path_utils.c src\path_utils.h src\mem.h build
	cl /c $(CLFLAGS) src\path_utils.c
build\cli.obj: src\cli.c src\cli.h src\dynamic_string.h src\mem.h build
	cl /c $(CLFLAGS) src\cli.c
build\unicode_width.obj: src\unicode_width.c src\unicode_width.h build
	cl /c $(CLFLAGS) src\unicode_width.c
build\autocmp.obj: src\autocmp.c src\mem.h src\match_node.h src\subprocess.h src\args.h src\dynamic_string.h src\whashmap.h src\hashmap.h
	cl /c $(CLFLAGS) src\autocmp.c
build\glob.obj: src\glob.c src\glob.h src\dynamic_string.h src\mem.h build
	cl /c $(CLFLAGS) src\glob.c
build\hashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build
	cl /c $(CLFLAGS) /DHASHMAP_LINKED src\hashmap.c
build\whashmap.obj: src\hashmap.c src\hashmap.h src\whashmap.h src\mem.h build
	cl /c $(CLFLAGS) /Fo:build\whashmap.obj /DHASHMAP_WIDE /DHASHMAP_CASE_INSENSITIVE src\hashmap.c
build\json.obj: src\json.c src\json.h src\dynamic_string.h src\hashmap.h src\mem.h
	cl /c $(CLFLAGS) src\json.c
build\subprocess.obj: src\subprocess.c src\subprocess.h src\dynamic_string.h src\mem.h build
	cl /c $(CLFLAGS) src\subprocess.c
build\match_node.obj: src\match_node.c src\match_node.h src\subprocess.h src\args.h src\dynamic_string.h src\glob.h src\whashmap.h src\hashmap.h src\mem.h
	cl /c $(CLFLAGS) src\match_node.c
build\mem.obj: src\mem.c build
	cl /c $(CLFLAGS) src\mem.c


bin\pathc.exe: src\pathc.c build\path_utils.obj build\args.obj build\printf.obj build\dynamic_string.obj build\mem.obj build\ntutils.lib bin
	cl $(CLFLAGS) /Fe:bin\pathc.exe src\pathc.c build\printf.obj build\path_utils.obj build\args.obj build\dynamic_string.obj build\mem.obj build\ntutils.lib $(LINKFLAGS)
bin\parse-template.exe: src\parse-template.c build\args.obj build\dynamic_string.obj build\mem.obj build\ntutils.lib bin
	cl $(CLFLAGS) /Fe:bin\parse-template.exe src\parse-template.c build\args.obj build\dynamic_string.obj build\mem.obj build\ntutils.lib chkstk.obj $(LINKFLAGS)
bin\path-add.exe: src\path-add.c build\ntutils.lib build\path_utils.obj build\args.obj build\printf.obj build\dynamic_string.obj build\mem.obj bin
	cl $(CLFLAGS) /Fe:bin\path-add.exe src\path-add.c build\ntutils.lib build\path_utils.obj build\args.obj build\printf.obj build\mem.obj build\dynamic_string.obj $(LINKFLAGS)
bin\envir.exe: src\envir.c build\ntutils.lib build\printf.obj build\args.obj build\path_utils.obj build\dynamic_string.obj build\mem.obj bin
	cl $(CLFLAGS) /Fe:bin\envir.exe src\envir.c build\ntutils.lib build\printf.obj build\args.obj build\dynamic_string.obj build\mem.obj build\path_utils.obj $(LINKFLAGS)
bin\search.exe: src\search.c build\ntutils.lib build\printf.obj build\path_utils.obj build\dynamic_string.obj build\mem.obj build\args.obj bin
	cl $(CLFLAGS) /Fe:bin\search.exe src\search.c build\ntutils.lib build\printf.obj build\dynamic_string.obj build\args.obj build\mem.obj build\path_utils.obj User32.lib $(LINKFLAGS)
bin\picker.exe: src\picker.c build\printf.obj build\dynamic_string.obj build\ntutils.lib
	cl $(CLFLAGS) /D _WINDOWS /Fe:bin\picker.exe src\picker.c build\ntutils.lib build\printf.obj build\mem.obj build\dynamic_string.obj User32.lib Gdi32.lib $(LINKFLAGSW)
bin\path-edit.exe: src\path-edit.c build\cli.obj build\dynamic_string.obj build\printf.obj build\unicode_width.obj build\ntutils.lib
	cl $(CLFLAGS) /Fe:bin\path-edit.exe src\path-edit.c build\cli.obj build\printf.obj build\dynamic_string.obj build\mem.obj build\unicode_width.obj build\ntutils.lib chkstk.obj $(LINKFLAGS)
bin\path-select.exe: src\path-select.c build\printf.obj build\args.obj build\path_utils.obj build\dynamic_string.obj build\mem.obj build\ntutils.lib build\kernelbase.lib
	cl $(CLFLAGS) /Fe:bin\path-select.exe src\path-select.c build\printf.obj build\args.obj build\path_utils.obj build\dynamic_string.obj build\mem.obj build\ntutils.lib build\kernelbase.lib $(LINKFLAGS)
bin\inject.exe: src\inject.c build\printf.obj build\mem.obj build\glob.obj build\ntutils.lib
	cl $(CLFLAGS) /Fe:bin\inject.exe src\inject.c build\printf.obj build\glob.obj build\ntutils.lib chkstk.obj build\mem.obj build\dynamic_string.obj $(LINKFLAGS)

bin\autocmp.dll: build\autocmp.obj build\printf.obj build\dynamic_string.obj build\glob.obj build\match_node.obj build\subprocess.obj build\whashmap.obj build\args.obj build\hashmap.obj build\json.obj build\mem.obj build\ntutils.lib
	link /DLL /NODEFAULTLIB /entry:DLLMain /LTCG /OUT:bin\autocmp.dll build\autocmp.obj build\dynamic_string.obj build\printf.obj build\glob.obj build\mem.obj build\match_node.obj build\subprocess.obj build\whashmap.obj build\args.obj build\hashmap.obj build\json.obj chkstk.obj build\ntutils.lib Kernel32.lib
	

bin\test.exe: src\test.c build\match_node.obj build\glob.obj build\dynamic_string.obj build\printf.obj build\ntutils.lib build\hashmap.obj build\whashmap.obj build\mem.obj build\json.obj build\subprocess.obj build\args.obj
	cl $(CLFLAGS) /Fe:bin\test.exe src\test.c build\glob.obj build\dynamic_string.obj build\printf.obj build\hashmap.obj build\whashmap.obj build\json.obj build\subprocess.obj build\args.obj build\match_node.obj build\mem.obj chkstk.obj build\ntutils.lib $(LINKFLAGS)

build\whashmapd.obj: src\hashmap.c src\hashmap.h src\whashmap.h build
	gcc -g -Og -c -o build\whashmapd.obj -DHASHMAP_WIDE -DHASHMAP_CASE_INSENSITIVE src\hashmap.c

bin\testd.exe: src\test.c src\glob.c src\dynamic_string.c src\printf.c build\ntutils.lib src\hashmap.c build\whashmapd.obj src\json.c src\subprocess.c src\args.c src\match_node.c src\mem.c
	gcc -g -Og -o bin\testd.exe -DHASHMAP_LINKED src\test.c src\glob.c src\dynamic_string.c src\printf.c build\ntutils.lib src\hashmap.c build\whashmapd.obj src\json.c src\subprocess.c src\args.c src\match_node.c src\mem.c

build\kernelbase.lib: build
	lib /MACHINE:X64 /DEF /OUT:build\kernelbase.lib /NAME:kernelbase.dll\
	 /EXPORT:PathMatchSpecW=PathMatchSpecW

build\ntutils.lib: build
	lib /MACHINE:X64 /DEF /OUT:build\ntutils.lib /NAME:ntdll.dll\
	 /EXPORT:memcpy=memcpy /EXPORT:strlen=strlen /EXPORT:memmove=memmove\
	 /EXPORT:_wsplitpath_s=_wsplitpath_s /EXPORT:wcslen=wcslen\
	 /EXPORT:_wmakepath_s=_wmakepath_s /EXPORT:strchr=strchr /EXPORT:_stricmp=_stricmp\
	 /EXPORT:towlower=towlower /EXPORT:_wcsicmp=_wcsicmp /EXPORT:_snwprintf_s=_snwprintf_s\
	 /EXPORT:_snprintf_s=_snprintf_s /EXPORT:_vscwprintf=_vscwprintf /EXPORT:_vsnprintf:_vsnprintf\
	 /EXPORT:_vsnwprintf=_vsnwprintf /EXPORT:_vsnprintf=_vsnprintf /EXPORT:_vscprintf=_vscprintf\
	 /EXPORT:memset=memset /EXPORT:wcscmp=wcscmp /EXPORT:strcmp=strcmp\
	 /EXPORT:_fltused=_fltused /EXPORT:wcschr=wcschr /EXPORT:wcsrchr=wcsrchr\
	 /EXPORT:strtol=strtol /EXPORT:wcsncmp=wcsncmp /EXPORT:memcmp=memcmp\
	 /EXPORT:tolower=tolower /EXPORT:strncmp=strncmp /EXPORT:_snprintf=_snprintf

bin\err.exe: script\err.exe bin
	type script\err.exe > bin\err.exe
bin\2to3.bat: script\2to3.bat bin
	type script\2to3.bat > bin\2to3.bat
bin\cal.bat: script\cal.bat bin
	type script\cal.bat > bin\cal.bat
bin\cleanenv.bat: script\cleanenv.bat bin
	type script\cleanenv.bat > bin\cleanenv.bat
bin\clip2.bat: script\clip2.bat bin
	type script\clip2.bat > bin\clip2.bat
bin\exp.bat: script\exp.bat bin
	type script\exp.bat > bin\exp.bat
bin\grep.bat: script\grep.bat bin
	type script\grep.bat > bin\grep.bat
bin\ls.bat: script\ls.bat bin
	type script\ls.bat > bin\ls.bat
bin\path-add.bat: script\path-add.bat bin
	type script\path-add.bat > bin\path-add.bat
bin\pathc.bat: script\pathc.bat bin
	type script\pathc.bat > bin\pathc.bat
bin\ports.bat: script\ports.bat bin
	type script\ports.bat > bin\ports.bat
bin\short.bat: script\short.bat bin
	type script\short.bat > bin\short.bat
bin\unzip.bat: script\unzip.bat bin
	type script\unzip.bat > bin\unzip.bat
bin\wget.bat: script\wget.bat bin
	type script\wget.bat > bin\wget.bat
bin\xkcd.bat: script\xkcd.bat bin
	type script\xkcd.bat > bin\xkcd.bat
bin\cleanenv.ps1: script\cleanenv.ps1 bin
	type script\cleanenv.ps1 > bin\cleanenv.ps1
bin\path-add.ps1: script\path-add.ps1 bin
	type script\path-add.ps1 > bin\path-add.ps1
bin\vcvarsall.ps1: script\vcvarsall.ps1 bin
	type script\vcvarsall.ps1 > bin\vcvarsall.ps1
bin\xkcd-keywords.txt: script\xkcd-keywords.txt bin
	type script\xkcd-keywords.txt > bin\xkcd-keywords.txt
bin\xkcd-titles.txt: script\xkcd-titles.txt bin
	type script\xkcd-titles.txt > bin\xkcd-titles.txt


bin\autocmp.json: autocmp.json bin
	type autocmp.json > bin\autocmp.json

script\translations:
	@echo  File script\translations missing
	@echo  The following keys are needed: NPP_PATH, PASS_PATH, GIT_PATH and VCVARS_PATH

bin\cmdrc.bat: script\cmdrc.bat.template bin\parse-template.exe script\translations
	bin\parse-template.exe script\cmdrc.bat.template script\translations bin\cmdrc.bat
bin\password.bat: script\password.bat.template bin\parse-template.exe script\translations
	bin\parse-template.exe script\password.bat.template script\translations bin\password.bat
bin\vcvarsall.bat: script\vcvarsall.bat.template bin\parse-template.exe script\translations
	bin\parse-template.exe script\vcvarsall.bat.template script\translations bin\vcvarsall.bat

install: all
	-@ if exist "bin\short.exe" del "bin\short.exe"
	-@ if exist "bin\path-add.bat" del "bin\path-add.bat"
	-@ if exist "bin\path-add.ps1" del "bin\path-add.ps1"
	-@ if exist "bin\envheap" del "bin\envheap"
	-@ if exist "bin\paths.txt" del "bin\paths.txt"
	copy bin\* ..

clean:
	del build\* /Q
	del bin\* /Q
