@echo off
test\stdasm_test.exe
test\args_test1.exe
test\args_test2.exe     "Hello world" 1 2 3456789     nice   "run""that"
"%~dp0\test\args_test3.exe" abc "efgh ijkl" mnop qrstuv name