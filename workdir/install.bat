@echo off
call build.bat


del bin\short.exe
copy bin\* ..
