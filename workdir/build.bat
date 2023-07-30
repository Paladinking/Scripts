@setlocal
echo NASM
@call path-add nasm >nul
nasm -fwin64 src/short.asm -o build/short.obj && echo  build/short.obj
nasm -fwin64 src/stdasm.asm -o build/stdasm.obj && echo  build/stdasm.obj
nasm -fwin64 src/args.asm -o build/args.obj && echo  build/args.obj
nasm -fwin64 src/chkstk.asm -o build/chkstk.a && echo  build/chkstk.a
@endlocal
@setlocal
echo MSVC
@call path-add msvc >nul
call :cl_build bin/short.exe build/short.obj build/stdasm.obj build/args.obj Kernel32.lib /link /NODEFAULTLIB /entry:main
call :cl_build test/stdasm_test.exe build/stdasm.obj Kernel32.lib chkstk.obj src/stdasm_test.c /GS- /link /NODEFAULTLIB /entry:main
call :cl_build test/args_test1.exe build/stdasm.obj build/args.obj Kernel32.lib chkstk.obj src/args_test.c /GS- /link /NODEFAULTLIB /entry:parse_args_1 /SUBSYSTEM:CONSOLE
call :cl_build test/args_test2.exe build/stdasm.obj build/args.obj Kernel32.lib chkstk.obj src/args_test.c /GS- /link /NODEFAULTLIB /entry:parse_args_2 /SUBSYSTEM:CONSOLE
call :cl_build test/args_test3.exe build/stdasm.obj build/args.obj Kernel32.lib chkstk.obj src/args_test.c /GS- /link /NODEFAULTLIB /entry:parse_args_3 /SUBSYSTEM:CONSOLE
goto :msvc_end
:cl_build
set OUT_FILE=%1
shift /1
cl /Fe:%* > build_out.txt 2>&1
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
exit /b
@setlocal
echo MINGW
@call path-add mingw >nul
gcc src/stdasm_test.c build/stdasm.obj build/chkstk.a -lKernel32 --entry=entry_main -nostdlib -o test/stdasm_test-gcc.exe && echo  test/stdasm_test-gcc.exe
@endlocal


