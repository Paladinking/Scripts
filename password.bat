@echo off
setlocal
set wd=%cd%
cd C:\Users\axelh\code\Pass
python pswrd.py %1 %2 %3 %4 %5 %6 %7 %8 %9
cd %wd%
