@echo off
:: Regedit HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Command Processor add key AutoRun pointing to this file
doskey notepad++="C:\Program files (x86)\Notepad++\notepad++.exe" $*
doskey clear=cls
doskey cat=type $*
doskey mv=move $*
doskey cp=copy $*
doskey rm=del $*
