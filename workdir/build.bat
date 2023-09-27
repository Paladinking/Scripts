@echo off
@setlocal
echo NASM
nasm -fwin64 src/short.asm -o build/short.obj && echo  build/short.obj
nasm -fwin64 src/stdasm.asm -o build/stdasm.obj && echo  build/stdasm.obj
nasm -fwin64 src/args.asm -o build/args.obj && echo  build/args.obj
nasm -fwin64 src/chkstk.asm -o build/chkstk.a && echo  build/chkstk.a
@endlocal
@setlocal
echo MSVC
cl /c /Fo:build\dynamic_string.obj src/dynamic_string.c /GS- /link /NODEFAULTLIB
call :cl_build bin/short.exe build/short.obj build/stdasm.obj build/args.obj Kernel32.lib /link /NODEFAULTLIB /entry:main
call :cl_build bin/addpath.exe src/addpath.c Kernel32.lib Shell32.lib chkstk.obj build/args.obj /GS- /link /NODEFAULTLIB /entry:main
call :cl_build bin/pathc.exe src/pathc.c Kernel32.lib Shell32.lib chkstk.obj /GS- /link /NODEFAULTLIB /entry:main
call :cl_build bin/parse-template.exe src/parse-template.c build/dynamic_string.obj Kernel32.lib Shell32.lib chkstk.obj /GS- /link /NODEFAULTLIB /entry:main
call :cl_build bin/regget.exe src/regget.c build/args.obj Kernel32.lib Advapi32.lib /GS- /link /NODEFAULTLIB /entry:main
call :cl_build bin/envget.exe src/envget.c Advapi32.lib Userenv.lib Kernel32.lib /GS- /link /NODEFAULTLIB /entry:main
call :cl_build test/stdasm_test.exe build/stdasm.obj Kernel32.lib chkstk.obj src/stdasm_test.c /GS- /link /NODEFAULTLIB /entry:main
call :cl_build test/args_test1.exe build/stdasm.obj build/args.obj Kernel32.lib chkstk.obj src/args_test.c /GS- /link /NODEFAULTLIB /entry:parse_args_1 /SUBSYSTEM:CONSOLE
call :cl_build test/args_test2.exe build/stdasm.obj build/args.obj Kernel32.lib chkstk.obj src/args_test.c /GS- /link /NODEFAULTLIB /entry:parse_args_2 /SUBSYSTEM:CONSOLE
call :cl_build test/args_test3.exe build/stdasm.obj build/args.obj Kernel32.lib chkstk.obj src/args_test.c /GS- /link /NODEFAULTLIB /entry:parse_args_3 /SUBSYSTEM:CONSOLE
goto :msvc_end
:cl_build
set OUT_FILE=%1
cl /Fo:build\ /Fe:%*
if [%errorlevel%] NEQ [0] (
	type build_out.txt
) else (
	echo  %OUT_FILE%
)
if exist build_out.txt (
	del build_out.txt
)
exit /b

:msvc_end
@endlocal
for %%f in (script\*.template) do (
	bin\parse-template.exe %%f script\translations bin\%%~nf
)
for %%f in (script\*.bat) do (
	copy %%f bin\%%~nf.bat
)
for %%f in (script\*.ps1) do (
	copy %%f bin\%%~nf.ps1
)
for %%f in (script\*.txt) do (
	copy %%f bin\%%~nf.txt
)
for %%f in (script\*.exe) do (
	copy %%f bin\%%~nf.exes
)


exit /b
@setlocal
echo MINGW
gcc src/stdasm_test.c build/stdasm.obj build/chkstk.a -lKernel32 --entry=entry_main -nostdlib -o test/stdasm_test-gcc.exe && echo  test/stdasm_test-gcc.exe
gcc src/envget.c -o bin/envget-gcc.exe -lUserenv && echo bin/envget-gcc.exe
gcc src/pathc.c -o bin/pathc-gcc.exe -lKernel32 -lShell32 build/chkstk.a -nostdlib --entry=entry_main
@endlocal


