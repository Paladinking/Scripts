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
cl build/short.obj build/stdasm.obj build/args.obj Kernel32.lib /Fe:bin/short.exe /link /NODEFAULTLIB /entry:main > build_out.txt 2>&1
if [%errorlevel%] NEQ [0] (
	type build_out.txt
) else (
	echo  bin/short.exe
)
cl build/stdasm.obj build/args.obj Kernel32.lib chkstk.obj src/stdasm_test.c /GS- /Fe:test\stdasm_test.exe /link /NODEFAULTLIB /entry:main > build_out.txt 2>&1
if [%errorlevel%] NEQ [0] (
	type build_out.txt
) else (
	echo  test/stdasm_test.exe
	test\stdasm_test.exe
)
if exist build_out.txt (
	del build_out.txt
)
@endlocal
@setlocal
echo MINGW
@call path-add mingw >nul
gcc src/stdasm_test.c build/stdasm.obj build/chkstk.a -lKernel32 --entry=entry_main -nostdlib -o test/stdasm_test-gcc.exe && echo  test/stdasm_test-gcc.exe && test\stdasm_test-gcc.exe
@endlocal


