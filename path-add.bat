@echo off

if "%~1" EQU "" (
	exit /b 2
)
:loop
echo ^^^|RESET| findstr /i "|%~1\>" >nul
if [%errorlevel%] EQU [0] (
	path %~dp0;C:\Windows\System32;C:\Windows;
	set "VSCMD_VER="
)

for /f "delims=<" %%i in ('findstr /i "<%~1\>" %~dp0paths.txt') do (
	call path-add %%i
)
for /f "delims=>" %%i in ('findstr /i ">%~1\>" %~dp0paths.txt') do (
	call %%i
)
for /f "delims=|" %%i in ('findstr /i "|%~1\>" %~dp0paths.txt') do (
	call :setpath %%i
)
shift /1
if "%~1" EQU "" (
	exit /b 0
)
goto :loop

:setpath
set PATH_ADD_NEW_PATH="%~1"
call addPath PATH_ADD_NEW_PATH /B
if [%errorlevel%] EQU [0] (
	echo Added "%~1" to path
)
set PATH_ADD_NEW_PATH=

exit /b 0