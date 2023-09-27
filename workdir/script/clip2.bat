@echo off
@setlocal

if "%~1" NEQ "" (
	set CLIP_VAR=%1
) else (
	for /f "tokens=*" %%i in ('findstr .') do (
		set CLIP_VAR=%%i
	)
)

if not defined errorlevel (
	set errorlevel=0
)
set OLD_ERROR=%errorlevel%
<nul set /p =%CLIP_VAR%|clip
exit /b %OLD_ERROR%
