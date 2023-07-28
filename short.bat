@echo off
if [%1] EQU [] (
	call :print "%cd%"
) else (
	call :print "%~1"
)
exit /B

:print
echo %~s1