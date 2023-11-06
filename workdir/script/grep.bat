@echo off
for /f "tokens=*" %%i in ('where git 2^>nul') do (
	if exist "%%~dpi..\usr\bin\grep.exe" (
		"%%~dpi..\usr\bin\grep.exe" %*
		exit /b
	)
)

@setlocal EnableDelayedExpansion

set RECURSIVE=FALSE
set CASE=
set NUMBER=
set REVERSE=
set STR_TO_FIND=
set NO_STR_TO_FIND=TRUE
set FILE=
set NO_FILE=TRUE

:loop
if "%~1" NEQ "" (
	shift
	if "%~1" EQU "-n" (
		set NUMBER=/N 
		goto :loop
	)
	if "%~1" EQU "-r" (
		set RECURSIVE=TRUE
		goto :loop
	)
	if "%~1" EQU "-i" (
		set CASE=/I 
		goto :loop
	)
	if "%~1" EQU "-v" (
		set REVERSE=/V 
		goto :loop
	)
	if [!STR_TO_FIND!] EQU [] (
		set STR_TO_FIND=/C:"%~1"
		set NO_STR_TO_FIND=FALSE
	) else (
		set FILE="%~1" !FILE!
		set NO_FILE=FALSE
	)
	goto :loop
)
if %RECURSIVE% EQU TRUE (
	set NEW_FILE=
	if %NO_STR_TO_FIND% EQU TRUE (
		echo Missing string to match
		exit /B
	)
	if %NO_FILE% EQU TRUE (
		set NEW_FILE=*
	) else (
		FOR %%A IN (%FILE%) do (
			call :attributes %%A
			set NEW_FILE=!OUT! !NEW_FILE!
		)
	)
	findstr %NUMBER%%CASE%%REVERSE%/P /S %STR_TO_FIND% !NEW_FILE!
) else (
	findstr %NUMBER%%CASE%%REVERSE%/P %STR_TO_FIND% %FILE%
)

exit /B

:attributes
set ATTR=%~a1
set DIRATTR=%ATTR:~0,1%
if /I "%DIRATTR%"=="d" (
	set OUT=%1\*
) else (
	set OUT=%1
)
exit /B
