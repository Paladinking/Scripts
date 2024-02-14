@echo off

call build.bat

if exist bin\short.exe (
	del bin\short.exe
)
del bin\path-add.bat
copy bin\* ..
