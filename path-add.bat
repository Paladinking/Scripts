@echo off

if "%~1" EQU "" (
	exit /b 2
)

if not defined SYSTEMROOT (
	set SYSTEMROOT=C:\Windows
)

:loop
echo ^^^|CLEAN| %SYSTEMROOT%\System32\findstr /i "|%~1\>" >nul
if [%errorlevel%] EQU [0] (
	path %~dp0;%SYSTEMROOT%\System32;%SYSTEMROOT%;
	echo Cleaned path
)

echo ^^^|RESET| %SYSTEMROOT%\System32\findstr /i "|%~1\>" >nul
if [%errorlevel%] EQU [0] (
	call cleanenv.bat
	echo Reset environment
)


for /f "delims=<" %%i in ('findstr /i "<%~1\>" %~dp0paths.txt') do (
	call path-add %%i
)
for /f "delims=>" %%i in ('findstr /i ">%~1\>" %~dp0paths.txt') do (
	call %%i
)
for /f "delims=|" %%i in ('findstr /i "|%~1\>" %~dp0paths.txt') do (
	call :add_path %%i
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