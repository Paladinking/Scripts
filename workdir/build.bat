@echo off
@setlocal EnableDelayedExpansion
set VERBOSITY=1
if "%~1" NEQ "" (
	set VERBOSITY=%~1
)

if not exist build (
	mkdir build
	call :echo_standard "Created build directory"
) else (
	call :echo_verbose "Skipped creating build directory"
)
if not exist bin (
	mkdir bin
	call :echo_standard "Created bin directory"
) else (
	call :echo_verbose "Skipped creating bin directory"
)


call :echo_standard NASM
set NASM_INPUTS=src\short.asm src\stdasm.asm src\args.asm

for %%a in (%NASM_INPUTS%) do (
	call :run_verbose nasm -fwin64 %%~a -o build\%%~na.obj
	call :echo_standard " build\%%~na.obj"
)


call :echo_standard MSVC
set CL_FLAGS=/Fo:build\ /GS- /GL /O1 /favor:AMD64
if %VERBOSITY% LEQ 2 (
	set CL_FLAGS=%CL_FLAGS% /nologo
)
set LINK_FLAGS=/link /NODEFAULTLIB /SUBSYSTEM:CONSOLE /LTCG /EMITPOGOPHASEINFO
set OBJECTS="src/dynamic_string.c"
set EXES="build\short.obj build\stdasm.obj build\args.obj Kernel32.lib"^
	"src\addpath.c Kernel32.lib Shell32.lib chkstk.obj build/args.obj"^
	"src\pathc.c Kernel32.lib Shell32.lib chkstk.obj"^
	"src\parse-template.c build/dynamic_string.obj build/memcp.lib Kernel32.lib Shell32.lib chkstk.obj"^
	"src\regget.c build\args.obj Kernel32.lib Advapi32.lib"

set LIB_CMD=lib /MACHINE:X64 /DEF /EXPORT:memcpy=memcpy /NAME:ntdll.dll /OUT:build\memcp.lib
if %VERBOSITY% LEQ 2 (
	set LIB_CMD=%LIB_CMD% /nologo
)
if %VERBOSITY% LEQ 1 (
	%LIB_CMD% > nul
) else (
	call :run_verbose %LIB_CMD%
)
call :echo_standard " build\memcp.lib"
call :run_verbose del build\memcp.exp

for %%a in (%OBJECTS%) do (
	call :compile_obj %%~a
)

for %%a in (%EXES%) do (
	call :compile_exe %%~a
)


call :echo_standard SCRIPTS

if not exist script\translations (
	echo  File script\translations missing
	echo  The following keys are needed: NPP_PATH, PASS_PATH and VCVARS_PATH
) else (
	for %%f in (script\*.template) do (
		call :run_verbose bin\parse-template.exe %%f script\translations bin\%%~nf
		if "!errorlevel!" EQU "0" (
			echo  bin\%%~nf
		)
	)
)

set FILE_COUNT=0

for %%f in (script\*.bat script\*.ps1 script\*.txt script\*.exe) do (
	copy %%f bin > nul
	call :echo_verbose " Copied %%f to bin"
	set /a "FILE_COUNT+=1"
)
call :echo_standard " Copied %FILE_COUNT% files."

exit /b

:compile_obj
set COMPILE_CMD=cl /c %* %CL_FLAGS% %LINK_FLAGS%
if %VERBOSITY% LEQ 1 (
	%COMPILE_CMD% >nul 2>&1
	if "!errorlevel!" EQU "0" (
		call :echo_standard " build\%~n1.obj"
	) else (
		if %VERBOSITY% EQU 1 (
			%COMPILE_CMD%
		)
	)
) else (
	call :run_verbose %COMPILE_CMD%
	if "!errorlevel!" EQU "0" (
		echo  build\%~n1.obj"
	)
)
exit /b

:compile_exe
set COMPILE_CMD=cl /Fe:bin\%~n1.exe %* %CL_FLAGS% %LINK_FLAGS% /entry:main
if %VERBOSITY% LEQ 1 (
	%COMPILE_CMD% >nul 2>&1
	if "!errorlevel!" EQU "0" (
		call :echo_standard " bin\%~n1.exe"
	) else (
		if %VERBOSITY% EQU 1 (
			%COMPILE_CMD%
		)
	)
) else (
	call :run_verbose %COMPILE_CMD%
	if "!errorlevel!" EQU "0" (
		echo  bin\%~n1.exe"
	)
)
exit /b

:echo_standard
if %VERBOSITY% NEQ 0 (
	echo %~1
)
exit /b

:run_verbose
if %VERBOSITY% GEQ 2 (
	echo %*
)
%*
exit /b

:echo_verbose
if %VERBOSITY% GEQ 2 (
	echo %~1
)
exit /b


@setlocal
echo MINGW
gcc src/stdasm_test.c build/stdasm.obj build/chkstk.a -lKernel32 --entry=entry_main -nostdlib -o test/stdasm_test-gcc.exe && echo  test/stdasm_test-gcc.exe
gcc src/envget.c -o bin/envget-gcc.exe -lUserenv && echo bin/envget-gcc.exe
gcc src/pathc.c -o bin/pathc-gcc.exe -lKernel32 -lShell32 build/chkstk.a -nostdlib --entry=entry_main
@endlocal


