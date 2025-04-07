all: \
	bin-gcc\pathc.exe \
	bin-gcc\parse-template.exe \
	bin-gcc\path-add.exe \
	bin-gcc\envir.exe \
	bin-gcc\path-edit.exe \
	bin-gcc\path-select.exe \
	bin-gcc\inject.exe \
	bin-gcc\list-dir.exe \
	bin-gcc\autocmp.dll \
	bin-gcc\test.exe \
	bin-gcc\symbol-dump.exe \
	bin-gcc\symbol-scrape.exe \
	bin-gcc\defer.exe \
	bin-gcc\find-file.exe \
	bin-gcc\type-file.exe \
	bin-gcc\autocmp.json \
	bin-gcc\err.exe \
	bin-gcc\2to3.bat \
	bin-gcc\cal.bat \
	bin-gcc\ports.bat \
	bin-gcc\short.bat \
	bin-gcc\wget.bat \
	bin-gcc\xkcd.bat \
	bin-gcc\xkcd-keywords.txt \
	bin-gcc\xkcd-titles.txt \
	bin-gcc\vcvarsall.ps1 \
	bin-gcc\cmdrc.bat \
	bin-gcc\password.bat \
	bin-gcc\vcvarsall.bat

build-gcc:
	-@ if NOT EXIST "build-gcc" mkdir "build-gcc"
bin-gcc:
	-@ if NOT EXIST "bin-gcc" mkdir "bin-gcc"
script:
	-@ if NOT EXIST "script" mkdir "script"

build-gcc\pathc.obj: src\pathc.c src\args.h src\path_utils.h src\dynamic_string.h src\mem.h src\printf.h build-gcc
	gcc.exe -c -o build-gcc\pathc.obj -g -Og src\pathc.c
build-gcc\path_utils.obj: src\path_utils.c src\path_utils.h src\dynamic_string.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\path_utils.obj -g -Og src\path_utils.c
build-gcc\args.obj: src\args.c src\args.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\args.obj -g -Og src\args.c
build-gcc\mem.obj: src\mem.c src\mem.h build-gcc
	gcc.exe -c -o build-gcc\mem.obj -g -Og src\mem.c
build-gcc\dynamic_string.obj: src\dynamic_string.c src\dynamic_string.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\dynamic_string.obj -g -Og src\dynamic_string.c
build-gcc\printf.obj: src\printf.c src\printf.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\printf.obj -g -Og src\printf.c
build-gcc\parse-template.obj: src\parse-template.c src\args.h src\mem.h src\dynamic_string.h build-gcc
	gcc.exe -c -o build-gcc\parse-template.obj -g -Og src\parse-template.c
build-gcc\path-add.obj: src\path-add.c src\args.h src\mem.h src\dynamic_string.h src\path_utils.h src\printf.h build-gcc
	gcc.exe -c -o build-gcc\path-add.obj -g -Og src\path-add.c
build-gcc\envir.obj: src\envir.c src\printf.h src\args.h src\path_utils.h src\dynamic_string.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\envir.obj -g -Og src\envir.c
build-gcc\path-edit.obj: src\path-edit.c src\cli.h src\dynamic_string.h src\mem.h src\printf.h build-gcc
	gcc.exe -c -o build-gcc\path-edit.obj -g -Og src\path-edit.c
build-gcc\cli.obj: src\cli.c src\cli.h src\dynamic_string.h src\mem.h src\printf.h src\unicode_width.h build-gcc
	gcc.exe -c -o build-gcc\cli.obj -g -Og src\cli.c
build-gcc\unicode_width.obj: src\unicode_width.c src\unicode_width.h build-gcc
	gcc.exe -c -o build-gcc\unicode_width.obj -g -Og src\unicode_width.c
build-gcc\path-select.obj: src\path-select.c src\args.h src\printf.h src\mem.h src\path_utils.h src\dynamic_string.h build-gcc
	gcc.exe -c -o build-gcc\path-select.obj -g -Og src\path-select.c
build-gcc\inject.obj: src\inject.c src\printf.h src\args.h src\glob.h src\dynamic_string.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\inject.obj -g -Og src\inject.c
build-gcc\glob.obj: src\glob.c src\glob.h src\dynamic_string.h src\mem.h src\args.h build-gcc
	gcc.exe -c -o build-gcc\glob.obj -g -Og src\glob.c
build-gcc\list-dir.obj: src\list-dir.c src\args.h src\glob.h src\dynamic_string.h src\mem.h src\perm.h src\printf.h src\unicode_width.h build-gcc
	gcc.exe -c -o build-gcc\list-dir.obj -g -Og src\list-dir.c
build-gcc\perm.obj: src\perm.c src\perm.h src\dynamic_string.h src\mem.h src\printf.h build-gcc
	gcc.exe -c -o build-gcc\perm.obj -g -Og src\perm.c
build-gcc\whashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\whashmap.obj -g -Og -DHASHMAP_WIDE -DHASHMAP_CASE_INSENSITIVE src\hashmap.c
build-gcc\lhashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\lhashmap.obj -g -Og -DHASHMAP_LINKED src\hashmap.c
build-gcc\hashmap.obj: src\hashmap.c src\hashmap.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\hashmap.obj -g -Og src\hashmap.c
build-gcc\autocmp.obj: src\autocmp.c src\printf.h src\dynamic_string.h src\mem.h src\json.h src\linkedhashmap.h src\hashmap.h src\match_node.h src\glob.h src\whashmap.h src\path_utils.h src\subprocess.h src\cli.h build-gcc
	gcc.exe -c -o build-gcc\autocmp.obj -g -Og src\autocmp.c
build-gcc\match_node.obj: src\match_node.c src\match_node.h src\glob.h src\dynamic_string.h src\mem.h src\whashmap.h src\hashmap.h src\subprocess.h src\args.h src\printf.h build-gcc
	gcc.exe -c -o build-gcc\match_node.obj -g -Og src\match_node.c
build-gcc\subprocess.obj: src\subprocess.c src\subprocess.h src\dynamic_string.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\subprocess.obj -g -Og src\subprocess.c
build-gcc\json.obj: src\json.c src\json.h src\linkedhashmap.h src\hashmap.h src\mem.h src\dynamic_string.h build-gcc
	gcc.exe -c -o build-gcc\json.obj -g -Og src\json.c
build-gcc\test.obj: src\test.c src\args.h src\regex.h src\glob.h src\dynamic_string.h src\mem.h src\printf.h build-gcc
	gcc.exe -c -o build-gcc\test.obj -g -Og src\test.c
build-gcc\regex.obj: src\regex.c src\regex.h src\dynamic_string.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\regex.obj -g -Og src\regex.c
build-gcc\symbol-dump.obj: src\symbol-dump.c src\coff.h src\printf.h src\args.h src\mem.h build-gcc
	gcc.exe -c -o build-gcc\symbol-dump.obj -g -Og src\symbol-dump.c
build-gcc\coff.obj: src\coff.c src\mem.h src\coff.h src\args.h build-gcc
	gcc.exe -c -o build-gcc\coff.obj -g -Og src\coff.c
build-gcc\symbol-scrape.obj: src\symbol-scrape.c src\args.h src\coff.h src\glob.h src\dynamic_string.h src\mem.h src\hashmap.h src\path_utils.h src\printf.h src\whashmap.h build-gcc
	gcc.exe -c -o build-gcc\symbol-scrape.obj -g -Og src\symbol-scrape.c
build-gcc\defer.obj: src\defer.c src\dynamic_string.h src\mem.h src\args.h src\glob.h src\printf.h src\subprocess.h build-gcc
	gcc.exe -c -o build-gcc\defer.obj -g -Og src\defer.c
build-gcc\find-file.obj: src\find-file.c src\args.h src\mem.h src\glob.h src\dynamic_string.h src\printf.h build-gcc
	gcc.exe -c -o build-gcc\find-file.obj -g -Og src\find-file.c
build-gcc\type-file.obj: src\type-file.c src\args.h src\glob.h src\dynamic_string.h src\mem.h src\printf.h build-gcc
	gcc.exe -c -o build-gcc\type-file.obj -g -Og src\type-file.c

bin-gcc\pathc.exe: build-gcc\pathc.obj build-gcc\path_utils.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\pathc.exe -g -Og build-gcc\pathc.obj build-gcc\path_utils.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib
bin-gcc\parse-template.exe: build-gcc\parse-template.obj build-gcc\args.obj build-gcc\dynamic_string.obj build-gcc\mem.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\parse-template.exe -g -Og build-gcc\parse-template.obj build-gcc\args.obj build-gcc\dynamic_string.obj build-gcc\mem.obj build-gcc\ntutils.lib
bin-gcc\path-add.exe: build-gcc\path-add.obj build-gcc\path_utils.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\path-add.exe -g -Og build-gcc\path-add.obj build-gcc\path_utils.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib
bin-gcc\envir.exe: build-gcc\envir.obj build-gcc\path_utils.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\envir.exe -g -Og build-gcc\envir.obj build-gcc\path_utils.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib
bin-gcc\path-edit.exe: build-gcc\path-edit.obj build-gcc\cli.obj build-gcc\unicode_width.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\path-edit.exe -g -Og build-gcc\path-edit.obj build-gcc\cli.obj build-gcc\unicode_width.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib
bin-gcc\path-select.exe: build-gcc\path-select.obj build-gcc\path_utils.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib build-gcc\kernelbase.lib bin-gcc
	gcc.exe -o bin-gcc\path-select.exe -g -Og build-gcc\path-select.obj build-gcc\path_utils.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib build-gcc\kernelbase.lib
bin-gcc\inject.exe: build-gcc\inject.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\glob.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\inject.exe -g -Og build-gcc\inject.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\glob.obj build-gcc\ntutils.lib
bin-gcc\list-dir.exe: build-gcc\list-dir.obj build-gcc\args.obj build-gcc\printf.obj build-gcc\perm.obj build-gcc\glob.obj build-gcc\dynamic_string.obj build-gcc\mem.obj build-gcc\unicode_width.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\list-dir.exe -g -Og -ladvapi32 build-gcc\list-dir.obj build-gcc\args.obj build-gcc\printf.obj build-gcc\perm.obj build-gcc\glob.obj build-gcc\dynamic_string.obj build-gcc\mem.obj build-gcc\unicode_width.obj build-gcc\ntutils.lib
bin-gcc\autocmp.dll: build-gcc\autocmp.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\match_node.obj build-gcc\subprocess.obj build-gcc\whashmap.obj build-gcc\lhashmap.obj build-gcc\json.obj build-gcc\cli.obj build-gcc\glob.obj build-gcc\path_utils.obj build-gcc\unicode_width.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -shared -o bin-gcc\autocmp.dll -g -Og build-gcc\autocmp.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\match_node.obj build-gcc\subprocess.obj build-gcc\whashmap.obj build-gcc\lhashmap.obj build-gcc\json.obj build-gcc\cli.obj build-gcc\glob.obj build-gcc\path_utils.obj build-gcc\unicode_width.obj build-gcc\ntutils.lib
bin-gcc\test.exe: build-gcc\test.obj build-gcc\glob.obj build-gcc\regex.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\test.exe -g -Og build-gcc\test.obj build-gcc\glob.obj build-gcc\regex.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib
bin-gcc\symbol-dump.exe: build-gcc\symbol-dump.obj build-gcc\coff.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\symbol-dump.exe -g -Og build-gcc\symbol-dump.obj build-gcc\coff.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib
bin-gcc\symbol-scrape.exe: build-gcc\symbol-scrape.obj build-gcc\path_utils.obj build-gcc\glob.obj build-gcc\coff.obj build-gcc\whashmap.obj build-gcc\hashmap.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\symbol-scrape.exe -g -Og build-gcc\symbol-scrape.obj build-gcc\path_utils.obj build-gcc\glob.obj build-gcc\coff.obj build-gcc\whashmap.obj build-gcc\hashmap.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib
bin-gcc\defer.exe: build-gcc\defer.obj build-gcc\subprocess.obj build-gcc\glob.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\defer.exe -g -Og build-gcc\defer.obj build-gcc\subprocess.obj build-gcc\glob.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib
bin-gcc\find-file.exe: build-gcc\find-file.obj build-gcc\glob.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\find-file.exe -g -Og build-gcc\find-file.obj build-gcc\glob.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib
bin-gcc\type-file.exe: build-gcc\type-file.obj build-gcc\glob.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib bin-gcc
	gcc.exe -o bin-gcc\type-file.exe -g -Og build-gcc\type-file.obj build-gcc\glob.obj build-gcc\args.obj build-gcc\mem.obj build-gcc\dynamic_string.obj build-gcc\printf.obj build-gcc\ntutils.lib

build-gcc\ntutils.lib: build-gcc
	echo EXPORTS > build-gcc\ntutils.def
	echo memcpy >> build-gcc\ntutils.def
	echo strlen >> build-gcc\ntutils.def
	echo memmove >> build-gcc\ntutils.def
	echo _wsplitpath_s >> build-gcc\ntutils.def
	echo wcslen >> build-gcc\ntutils.def
	echo _wmakepath_s >> build-gcc\ntutils.def
	echo strchr >> build-gcc\ntutils.def
	echo _stricmp >> build-gcc\ntutils.def
	echo towlower >> build-gcc\ntutils.def
	echo _wcsicmp >> build-gcc\ntutils.def
	echo _snwprintf_s >> build-gcc\ntutils.def
	echo _snprintf_s >> build-gcc\ntutils.def
	echo _vscwprintf >> build-gcc\ntutils.def
	echo _vsnprintf >> build-gcc\ntutils.def
	echo _vsnwprintf >> build-gcc\ntutils.def
	echo _vscprintf >> build-gcc\ntutils.def
	echo memset >> build-gcc\ntutils.def
	echo wcscmp >> build-gcc\ntutils.def
	echo strcmp >> build-gcc\ntutils.def
	echo _fltused >> build-gcc\ntutils.def
	echo wcschr >> build-gcc\ntutils.def
	echo wcsrchr >> build-gcc\ntutils.def
	echo strtol >> build-gcc\ntutils.def
	echo wcsncmp >> build-gcc\ntutils.def
	echo memcmp >> build-gcc\ntutils.def
	echo tolower >> build-gcc\ntutils.def
	echo strncmp >> build-gcc\ntutils.def
	echo strnlen >> build-gcc\ntutils.def
	echo wcsstr >> build-gcc\ntutils.def
	echo _snprintf >> build-gcc\ntutils.def
	dlltool --dllname ntdll.dll --def build-gcc\ntutils.def --output-lib build-gcc\ntutils.lib
build-gcc\kernelbase.lib: build-gcc
	echo EXPORTS > build-gcc\kernelbase.def
	echo PathMatchSpecW >> build-gcc\kernelbase.def
	dlltool --dllname kernelbase.dll --def build-gcc\kernelbase.def --output-lib build-gcc\kernelbase.lib
bin-gcc\autocmp.json: autocmp.json bin-gcc
	type autocmp.json > bin-gcc\autocmp.json
bin-gcc\err.exe: script\err.exe bin-gcc
	type script\err.exe > bin-gcc\err.exe
bin-gcc\2to3.bat: script\2to3.bat bin-gcc
	type script\2to3.bat > bin-gcc\2to3.bat
bin-gcc\cal.bat: script\cal.bat bin-gcc
	type script\cal.bat > bin-gcc\cal.bat
bin-gcc\ports.bat: script\ports.bat bin-gcc
	type script\ports.bat > bin-gcc\ports.bat
bin-gcc\short.bat: script\short.bat bin-gcc
	type script\short.bat > bin-gcc\short.bat
bin-gcc\wget.bat: script\wget.bat bin-gcc
	type script\wget.bat > bin-gcc\wget.bat
bin-gcc\xkcd.bat: script\xkcd.bat bin-gcc
	type script\xkcd.bat > bin-gcc\xkcd.bat
bin-gcc\xkcd-keywords.txt: script\xkcd-keywords.txt bin-gcc
	type script\xkcd-keywords.txt > bin-gcc\xkcd-keywords.txt
bin-gcc\xkcd-titles.txt: script\xkcd-titles.txt bin-gcc
	type script\xkcd-titles.txt > bin-gcc\xkcd-titles.txt
bin-gcc\vcvarsall.ps1: script\vcvarsall.ps1 bin-gcc
	type script\vcvarsall.ps1 > bin-gcc\vcvarsall.ps1
script\translations: script
	@echo File script\translations missing
	@echo The following keys are needed: NPP_PATH, PASS_PATH, GIT_PATH, and VCVARS_PATH
bin-gcc\cmdrc.bat: script\cmdrc.bat.template bin-gcc\parse-template.exe script\translations bin-gcc
	bin-gcc\parse-template.exe script\cmdrc.bat.template script\translations bin-gcc\cmdrc.bat
bin-gcc\password.bat: script\password.bat.template bin-gcc\parse-template.exe script\translations bin-gcc
	bin-gcc\parse-template.exe script\password.bat.template script\translations bin-gcc\password.bat
bin-gcc\vcvarsall.bat: script\vcvarsall.bat.template bin-gcc\parse-template.exe script\translations bin-gcc
	bin-gcc\parse-template.exe script\vcvarsall.bat.template script\translations bin-gcc\vcvarsall.bat

clean:
	del /S /Q build-gcc\*
	del /S /Q bin-gcc\*
