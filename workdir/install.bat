@echo off

call build.bat


del bin\short.exe
del bin\path-add.bat
copy bin\* ..
