@echo off
call build.bat


copy bin\* ..
del bin\short.exe
