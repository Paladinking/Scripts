@echo off & python -x %~f0 %* & exit /B

from urllib.request import urlretrieve
import sys

def main():
    if len(sys.argv) < 2:
        print("Missing URL")
        return
    try:
        name_index = sys.argv.index("-O") + 1
        name = sys.argv[name_index]
    except Exception as e:
        print(e)
        if len(sys.argv) > 2:
            name = sys.argv[2]
        else:
            name = sys.argv[1].split('/')[-1]
    try:
        urlretrieve(sys.argv[1], filename=name)
    except Exception as e:
        print("Could not download", e)


if __name__ == "__main__":
    main()     
