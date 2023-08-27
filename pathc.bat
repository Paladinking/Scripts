@echo off
pathc.exe %* > nul
if [%errorlevel%] EQU [1] (
	exit /b 1
)
for /f tokens^=^*^ eol^= %%i in ('pathc.exe %*') do (
	path %%i
)
exit /b 0