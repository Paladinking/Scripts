@echo off

if "%~1" EQU "" (
	exit /B 2
)

echo ^^^|MSVC| findstr /i "|%~1\>" >nul
if [%errorlevel%] EQU [0] (
	if defined VSCMD_VER (
		exit /b 0
	)
	for /f "delims=|" %%i in ('findstr /i "|%~1\>" %~dp0paths.txt') do (
		call %%i
	)
	exit /b 0
)

for /f "delims=|" %%i in ('findstr /i "|%~1\>" %~dp0paths.txt') do (
	call :setpath %%i
)
exit /b 0

:setpath
set PATH_ADD_NEW_PATH="%~1"
call addPath PATH_ADD_NEW_PATH /B
if [%errorlevel%] EQU [0] (
	echo Added "%~1" to path
)
set PATH_ADD_NEW_PATH=

exit /b 0