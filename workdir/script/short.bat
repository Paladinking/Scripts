@echo off
if "%~1" EQU "" (
	call :print "%cd%"
	exit /b
)

if "%~1" EQU "-p" (
	for /f "tokens=*" %%i in ('findstr .') do (
		call :print "%%~i"
	)
	exit /b
)
	
call :print "%~1"
exit /B

:print
echo %~s1