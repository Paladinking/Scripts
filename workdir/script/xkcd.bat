@echo off
@setlocal

set URL=https://xkcd.com
set query=%*

if [%1] EQU [] (
    goto :default
)

echo %1| findstr /i /l /x random >nul
if %errorlevel% EQU 0 (
    start https://c.xkcd.com/random/comic
    goto :end
)

echo %1| findstr /i /l /x qsearch >nul
if %errorlevel% EQU 0 (
	if [%3] EQU [] (
		set query=%2
		findstr /i /n "\<%query:"=%\>" %~dp0xkcd-titles.txt
	) else (
		findstr /i /n /l /c:"%query:~8%" %~dp0xkcd-titles.txt
	)
    goto :end
)

echo %1| findstr /i /l /x explain >nul
if %errorlevel% EQU 0 (
    set URL=https://explainxkcd.com
    set query=%query:~8%
    shift
    if [%1] EQU [] (
	    goto :default
    )
)

if 1%1 EQU +1%1 (
    if [%2] EQU [] (
	    start %URL%/%1
	    goto :end
    )
)

if [%2] EQU [] (
	set FIND="\<%query:"=%\>"
) else (
	set FIND=/l /c:"%query:"=%"
)

for /f "delims=:" %%i in ('findstr /i /n /x %FIND% %~dp0xkcd-titles.txt') do (
    start %URL%/%%i
    goto :end
)

for /f "delims=:" %%i in ('findstr /i %FIND% %~dp0xkcd-keywords.txt') do (
	start %URL%/%%i
	goto :end
)

for /f "delims=:" %%i in ('findstr /i /n %FIND% %~dp0xkcd-titles.txt') do (
    start %URL%/%%i
    goto :end
)

:default
start %URL%

:end
