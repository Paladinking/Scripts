@echo off && python -x %~dpf0 %* & exit /b
import sys
import argparse
import glob
import os

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('file', help="Glob ")
    parser.add_argument('--convert', choices=['cr', 'lf', 'crlf'])

    args = parser.parse_args()

    if args.convert is not None:
        for arg in glob.glob(args.file):
            if not os.path.isfile(arg):
                continue
            try:
                with open(arg, 'r', encoding='utf-8') as file:
                    data = file.read().split('\n')
            except Exception:
                print(f"Could not open '{arg}'", file=sys.stderr)
                continue
            if not data[-1]:
                data = data[:-1]
            if args.convert == 'cr':
                eol = b'\r'
            elif args.convert == 'lf':
                eol = b'\n'
            else:
                eol = b'\r\n'
            with open(arg, 'wb') as file:
                for line in data:
                    file.write(line.encode('utf-8'))
                    file.write(eol)
        return


    for arg in glob.glob(args.file):
        if not os.path.isfile(arg):
            continue
        try:
            with open(arg, 'rb') as file:
                data = file.read()
        except Exception:
            print(f"Could not open '{arg}'", file=sys.stderr)
            continue
        has_cr = False
        has_lf = False
        has_crlf = False
        last = None
        for b in data:
            if b == b'\n'[0]:
                if last == b'\r'[0]:
                    has_crlf = True
                else:
                    has_lf = True
            elif last == b'\r'[0]:
                has_cr = True
            last = b
        print(f"{arg}: ", end="")
        if has_crlf and has_cr and has_lf:
            print("{\\r,\\n,\\r\\n}")
        elif has_cr and has_lf:
            print("{\\r,\\n}")
        elif has_cr and has_crlf:
            print("{\\r,\\r\\n}")
        elif has_crlf and has_lf:
            print("{\\n,\\r\\n}")
        elif has_crlf:
            print("\\r\\n")
        elif has_cr:
            print("\\r")
        elif has_lf:
            print("\\n")
        else:
            print("{}")


if __name__ == "__main__":
    main()
