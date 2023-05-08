@echo off & python -x %~f0 %* & exit /B
import sys
import subprocess

if __name__ == "__main__":
    res = subprocess.run(['dir', '/w', *sys.argv[1:]], shell = True, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    stdout = res.stdout.decode('cp437')
    if stdout:        
        if stdout.split('\n')[0].startswith(' Volume in drive'):
            lst = stdout.split('\n')
            lst = lst[5:-3]
            stdout = ''
            for line in lst:
                stdout += line + '\n'
        print(stdout, end='')
    stderr = res.stderr.decode('cp437')
    if stderr:
        print(stderr, end='')

