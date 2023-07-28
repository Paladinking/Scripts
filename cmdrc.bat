@echo off
:: Regedit HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Command Processor add key AutoRun pointing to this file
doskey notepad++="C:\Program files (x86)\Notepad++\notepad++.exe" $*
doskey ls=ls --color $*
doskey kill=taskkill /IM $*
doskey fkill=taskkill /F /IM $*