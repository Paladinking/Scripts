@setlocal
@call path-add nasm >nul
echo NASM
nasm -fwin64 short.asm -o short.obj && echo  short.obj
nasm -fwin64 stdasm.asm -o stdasm.obj && echo  stdasm.obj
nasm -fwin64 args.asm -o args.obj && echo  args.obj
nasm -fwin64 chkstk.asm -o chkstk.a && echo  chkstk.a
@endlocal
@setlocal
@call path-add msvc >nul
echo MSVC
cl short.obj stdasm.obj args.obj Kernel32.lib /link /NODEFAULTLIB /entry:main > build_out.txt 2>&1
if [%errorlevel%] NEQ [0] (
	type build_out.txt
) else (
	echo  short.exe
)
cl stdasm.obj args.obj Kernel32.lib chkstk.obj tests.c /GS- /Fe:tests.exe /link /NODEFAULTLIB /entry:main > build_out.txt 2>&1
if [%errorlevel%] NEQ [0] (
	type build_out.txt
) else (
	echo  tests.exe
	tests.exe
)
if exist build_out.txt (
	del build_out.txt
)
@endlocal
@setlocal
@call path-add mingw >nul
echo MINGW
gcc tests.c stdasm.obj chkstk.a -lKernel32 --entry=entry_main -nostdlib -o tests-gcc.exe && echo  tests-gcc.exe && tests-gcc.exe
@endlocal


