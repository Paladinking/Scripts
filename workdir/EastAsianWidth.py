from typing import List, Tuple
import sys

# Find smallest element where upper >= c
def get_part(ranges: List[Tuple[int, int]], known: bool, depth = 1) -> str:
    tab = "\t" * depth
    if len(ranges) == 1:
        if known:
            return tab + f"return c >= {ranges[0][0]};"
        return tab + f"return c <= {ranges[0][1]} && c >= {ranges[0][0]};"
    if len(ranges) == 2:
        if known:
            return tab + f"if (c <= {ranges[0][1]}) {{\n" + \
                   tab + f"\treturn c >= {ranges[0][0]};\n" + \
                   tab + f"}} else {{\n" + \
                   tab + f"\treturn c >= {ranges[1][0]};\n" + \
                   tab + f"}}"
        part = get_part(ranges[1:], False, depth + 1)
        return tab + f"if (c <= {ranges[0][1]}) {{\n" + \
               tab + f"\treturn c >= {ranges[0][0]};\n" + \
               tab + f"}} else {{\n" + \
                     f"{part}\n" + \
               tab + f"}}"
    split = len(ranges) // 2
    p1 = ranges[:split + 1]
    r = ranges[split]
    p2 = ranges[split + 1:]
    return tab + f"if (c <= {r[1]}) {{\n" + \
                 f"{get_part(p1, True, depth + 1)}\n" + \
           tab + f"}} else {{\n" + \
                 f"{get_part(p2, known, depth + 1)}\n" + \
           tab + f"}}"

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
    if not ranges:
        print("No data found", file=sys.stderr)
        exit(1)
    ranges = sorted(ranges)

    print("// File generated py EastAsianWidth.py")
    print("#include \"unicode_width.h\"\n")
    print("BOOL is_wide(UINT32 c) {")

    print(f'    if (c < {ranges[0][0]}) {{')
    print(f'        return FALSE;')
    print(f'    }}')

    print(get_part(ranges, False).replace('\t', '    '))
    print("}")

if __name__ == '__main__':
    main()

