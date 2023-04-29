@echo off
setlocal

set a1=%1
set a2=%2
set a3=%3
set a4=%4
set a5=%5
set a6=%6
set a7=%7
set a8=%8
set a9=%9

if [%1] EQU [] (
    goto end
)
where /Q %1
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %1') do (
        set a1=%%i
    )
)

if [%2] EQU [] (
    goto end
)
where /Q %2
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %2') do (
        set a2=%%i
    )
)

if [%3] EQU [] (
    goto end
)
where /Q %3
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %3') do (
        set a3=%%i
    )
)

if [%4] EQU [] (
    goto end
)
where /Q %4
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %4') do (
        set a4=%%i
    )
)

if [%5] EQU [] (
    goto end
)
where /Q %5
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %5') do (
        set a5=%%i
    )
)

if [%6] EQU [] (
    goto end
)
where /Q %6
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %6') do (
        set a6=%%i
    )
)

if [%6] EQU [] (
    goto end
)
where /Q %6
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %6') do (
        set a6=%%i
    )
)

if [%7] EQU [] (
    goto end
)
where /Q %7
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %7') do (
        set a7=%%i
    )
)
if [%8] EQU [] (
    goto end
)
where /Q %8
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %8') do (
        set a8=%%i
    )
)

if [%9] EQU [] (
    goto end
)
where /Q %9
if %errorlevel% EQU 0 (
    for /f "delims=*" %%i in ('where /F %9') do (
        set a9=%%i
    )
)

:end
%a1% %a2% %a3% %a4% %a5% %a6% %a7% %a8% %a9%
