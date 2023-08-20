@echo off

path C:\Windows\System32;C:\Windows;%PATH%

for /f "tokens=*" %%i in ('where envget') do (
	set EVNGET=%%~dpi
)
if [%errorlevel%] NEQ [0] (
	exit /b 1
)
pushd %EVNGET%

call :clear

for /f tokens^=*^ delims^=^ eol^= %%i in ('envget') do (
	set %%i
)

popd
exit /b 0
:clear
(
	for /f tokens^=1^ delims^=^=^ eol^= %%i in ('set') do (
		set %%i=
	)
	set "USERPROFILE=%USERPROFILE%"
	set "SystemRoot=%SystemRoot%"
	set "SystemDrive=%SYSTEMDRIVE%"
	set "ComSpec=%COMSPEC%"
	set "CommonProgramFiles=%CommonProgramFiles%"
	set "CommonProgramFiles(x86)=%CommonProgramFiles(x86)%"
	set "FPS_BROWSER_APP_PROFILE_STRING=%FPS_BROWSER_APP_PROFILE_STRING%"
	set "FPS_BROWSER_USER_PROFILE_STRING=%FPS_BROWSER_USER_PROFILE_STRING%"
	path C:\Windows\System32;C:\Windows
)
exit /b 0

