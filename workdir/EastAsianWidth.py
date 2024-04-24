from typing import List, Tuple

# Find smallest element where upper >= c
def get_part(ranges: List[Tuple[int, int]], known: bool) -> str:
    #print("//", ranges, known)
    if len(ranges) == 1:
        if known:
            return f"\treturn c >= {ranges[0][0]};"
        return f"\treturn c <= {ranges[0][1]} && c >= {ranges[0][0]};"
    if len(ranges) == 2:
        if known:
            return f"if (c <= {ranges[0][1]}) {{\n\treturn c >= {ranges[0][0]};\n}} else {{\n\treturn c >= {ranges[1][0]};\n}}"
        part = get_part(ranges[1:], False)
        return f"if (c <= {ranges[0][1]}) {{\n\treturn c >= {ranges[0][0]};\n}} else {{\n{part}\n}}"
    split = len(ranges) // 2
    p1 = ranges[:split + 1]
    r = ranges[split]
    p2 = ranges[split + 1:]
    return f"if (c <= {r[1]}) {{\n{get_part(p1, True)}\n}} else {{\n{get_part(p2, known)}\n}}"

def main() -> None:
    with open('EastAsianWidth.txt', 'r') as file:
        lines = [c.strip() for c in filter(lambda l: not l.strip().startswith('#'), file.read().split('\n'))]
    
    ranges: List[Tuple[int, int]] = []
    for line in lines:
        if not line:
            continue
        parts = line.split(';')
        urange = parts[0].strip().split('..')
        if len(urange) == 2:
            urange = (int(urange[0], 16), int(urange[1], 16))
        else:
            urange = (int(urange[0], 16), int(urange[0], 16))
        width = parts[1].split('#', 1)[0].strip()
        wide = width not in {'N', 'Na', 'H', 'A'}
        if wide:
            ranges.append(urange)
    print("//File generated py EastAsianWidth.py")
    print("#include <windows.h>")
    print("BOOL is_wide(UINT32 c) {")
    print(get_part(ranges, False))
    print("}")

if __name__ == '__main__':
    main()

