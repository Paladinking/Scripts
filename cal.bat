@echo off
setlocal

if [%1] EQU [] (
	goto :end
)

pushd C:\Users\axelh\Scripts

python cal.py %*

popd
:end
endlocal
