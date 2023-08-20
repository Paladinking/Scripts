@echo off
:: Regedit HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Command Processor add key AutoRun pointing to this file
doskey notepad++="C:\Program files (x86)\Notepad++\notepad++.exe" $*
doskey ls=ls --color $*
doskey kill=taskkill /IM $*
doskey fkill=taskkill /F /IM $*
doskey path-set=path-add RESET $*
doskey poetry=if "$1" EQU "enter" (for /f "tokens=*" %%i in ('poetry.exe env info --path') do @(%%i\Scripts\activate.bat)) else if "$1" EQU "leave" (for /f "tokens=*" %%i in ('poetry.exe env info --path') do @(%%i\Scripts\deactivate.bat)) else poetry.exe $*