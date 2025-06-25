import sys
from typing import List, Optional, Set, Tuple, Union, Dict


AnyRule = Union['Rule', str]


TOKEN_NAME = 0
TOKEN_TYPE = 1
TOKEN_INTEGER = 3
TOKEN_DOUBLE = 4
TOKEN_STRING = 5
TOKEN_LITERAL = 6
TOKEN_IDENTIFIER = 7
TOKEN_STATEMENTS = 8
TOKEN_EMPTY = 9

TOKENS = {TOKEN_NAME: 'NAME', TOKEN_TYPE: 'TYPE',
          TOKEN_INTEGER: 'INTEGER', TOKEN_DOUBLE: 'DOUBLE',
          TOKEN_STRING: 'STRING', TOKEN_LITERAL: 'LITERAL',
          TOKEN_IDENTIFIER: 'IDENTIFIER', TOKEN_STATEMENTS: 'STATEMENTS',
          TOKEN_EMPTY: 'EMPTY'}


SPACES = " \t\n\r\v"

def identifier_start(s: str) -> bool:
    return s in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"

def identifier_char(s: str) -> bool:
    return identifier_start(s) or s in "0123456789"

class ScanTokenizer:
    """
    Emits TOKEN_EMPTY, TOKEN_LITERAL, TOKEN_IDENTIFIER, and TOKEN_STATEMENTS
    """
    def __init__(self, s: str) -> None:
        self.s = s
        self.start = 0
        self.ix = 0

        self.get_token()

    def _skip_strliteral(self, c: str) -> None:
        while True:
            if self.ix >= len(self.s):
                break
            if self.s[self.ix] == c:
                self.ix += 1
                break
            if self.s[self.ix] == '\\':
                self.ix += 1
            self.ix += 1

    def peek_token(self):
        return self.token

    def get_token(self):
        self.token = self._get_token()
        return self.token

    def _get_token(self):
        if self.ix >= len(self.s):
            return (TOKEN_EMPTY, self.ix, self.ix)
        while self.s[self.ix] in SPACES:
            self.ix += 1
            if self.ix >= len(self.s):
                return (TOKEN_EMPTY, self.ix, self.ix)
        start = self.ix
        if self.s[self.ix] == '{':
            self.ix += 1
            count = 1
            while True:
                if self.ix >= len(self.s):
                    raise RuntimeError("Invalid statements")
                c = self.s[self.ix]
                self.ix += 1
                if c == '}':
                    count -= 1
                    if count == 0:
                        return (TOKEN_STATEMENTS, start, self.ix)
                elif c == '{':
                    count += 1
                elif c == '"':
                    self._skip_strliteral('"')
                elif c == '\'':
                    self._skip_strliteral('\'')
        if identifier_start(self.s[self.ix]):
            self.ix += 1
            while self.ix < len(self.s) and identifier_char(self.s[self.ix]):
                self.ix += 1
            if self.s[start:self.ix] == 'fn':
                return (TOKEN_LITERAL, start, self.ix)
            return (TOKEN_IDENTIFIER, start, self.ix)
        if self.s[self.ix] == '-':
            self.ix += 1
            if self.ix >= len(self.s):
                return (TOKEN_LITERAL, start, self.ix)
            if self.s[self.ix] == '>':
                self.ix += 1
            return (TOKEN_LITERAL, start, self.ix)
        self.ix += 1
        return (TOKEN_LITERAL, start, self.ix)


class Rule:
    def __init__(self, name: str, defparts: List[List[str]]) -> None:
        self.name = name
        self.defparts = defparts
        self.rules: List[List[AnyRule]] = []
    def __repr__(self) -> str:
        return "*" + self.name

class Node:
    def __init__(self, *data) -> None:
        self.rules: Tuple[Tuple[str, Tuple[Union[str, AnyRule], ...], Tuple[str, ...]], ...] = data
        self.edges: Dict[str, Node] = dict()
        self.expand()

    def get_reduction(self) -> Optional[str]:
        for name, red, lookahead in self.rules:
            if len(red) == 1:
                continue
            if red.index('.') == len(red) - 1:
                return name
        return None


    def __repr__(self) -> str:
        s = "(\n"
        for name, val, look in self.rules:
            s += f"{name} -> {val} - {look},\n"
        if s[-1] == '\n':
            s = s[:-2] + '\n)'
        else:
            s = s + ')'
        return s

    def __hash__(self) -> int:
        return hash(self.rules)

    def __eq__(self, other, /) -> bool:
        if not isinstance(other, Node):
            return False
        return self.rules == other.rules

    def add_children(self, graph: Dict['Node', 'Node']) -> None:
        for edge, node in self.children():
            if node in graph:
                self.edges[edge] = graph[node]
            else:
                self.edges[edge] = node
                graph[node] = node
                node.add_children(graph)

    def expand(self) -> None:
        new_rules = {r[:2]: r[2] for r in self.rules}
        changed = True
        while changed:
            changed = False
            for name, rule, lookahead in self.rules:
                ix = rule.index('.')
                if ix == len(rule) - 1:
                    continue
                r = rule[ix + 1]
                if not isinstance(r, Rule):
                    continue
                for vals in r.rules:
                    new: Tuple[str, Tuple[Union[str, AnyRule], ...]] = \
                            (r.name, ('.',) + tuple(vals))
                    if ix + 1 == len(rule) - 1:
                        look = lookahead
                    else:
                        next_rule = rule[ix + 2]
                        if isinstance(next_rule, Rule):
                            l = []
                            checked = {next_rule.name}
                            stack = [r for r in next_rule.rules]
                            while len(stack) > 0:
                                to_check = stack.pop()
                                if len(to_check) == 0:
                                    continue
                                to_check = to_check[0]
                                if not isinstance(to_check, Rule):
                                    l.append(str(to_check))
                                elif to_check.name not in checked:
                                    stack.extend([r for r in to_check.rules])
                                    checked.add(to_check.name)

                            look = tuple(l)
                        else:
                            look = (str(next_rule),)
                    if new not in new_rules:
                        changed = True
                        new_rules[new] = look
                    else:
                        old_look = new_rules[new]
                        look = tuple(sorted(tuple(set(look + old_look))))
                        new_rules[new] = look
            self.rules = tuple((t[0], t[1], v) for (t, v) in new_rules.items())


    def children(self) -> List[Tuple[str, 'Node']]:
        assert len(self.rules) > 0
        l = []
        consumed: Dict[str, Set[Tuple[str, Tuple[Union[AnyRule, str], ...], Tuple[str]]]] = dict()
        for name, val, lookahead in self.rules:
            ix = val.index('.')
            if ix == len(val) - 1:
                continue
            to_consume = val[ix + 1]
            new_val = (name, val[:ix] + (val[ix + 1],) + ('.',) + val[ix + 2:], lookahead)
            if isinstance(to_consume, Rule):
                n = to_consume.name
            else:
                n = str(to_consume)
            if n in consumed:
                consumed[n].add(new_val)
            else:
                consumed[n] = {new_val}

        for n, rules in consumed.items():
            node = Node(*rules)
            l.append((n, node))
        return l


def find_rule_end(s: str) -> int:
    ix = 0
    while ix < len(s):
        if s[ix] == ';':
            return ix
        if s[ix] == '\'':
            ix += 1
            while ix < len(s) and s[ix] != '\'':
                ix += 1 
        ix += 1
    return len(s)

def split_rule(s: str, char: str) -> List[str]:
    res = []
    ix = 0
    while ix < len(s):
        if s[ix] == '\'':
            ix += 1
            while ix < len(s) and s[ix] != '\'':
                ix += 1
        elif s[ix] == char:
            res.append(s[:ix].strip())
            s = s[ix + 1:]
            ix = 0
        ix += 1
    res.append(s.strip())
    return res

def main() -> None:
    if len(sys.argv) < 2:
        print("Missing argument", file=sys.stderr)
        exit(1)

    file = sys.argv[1]
    try:
        with open(file, 'r') as file:
            data = file.read()
    except OSError as e:
        print(f"Failed reading '{file}': {e}", file=sys.stderr)
        exit(1)

    lines = data.splitlines()

    lines = filter(lambda s: not s.strip().startswith('//'), lines)

    data = ''.join(lines)
    rules: Dict[str, AnyRule] = {}

    atoms: Dict[str, int] = {}
    literals: List[str] = []

    while data:
        end = find_rule_end(data)
        rule = data[:end].strip()
        data = data[end + 1:]

        if rule.startswith('atoms'):
            parts = rule.split(':', 1)
            for s in parts[1].split(','):
                s = s.strip()
                atoms[s] = len(atoms)
            continue

        parts = rule.split('=', 1)
        name = parts[0].strip()
        defs = split_rule(parts[1], '|')
        def_parts = []
        for d in defs:
            p = split_rule(d, '+')
            p = [s for s in p if s != "''"]
            def_parts.append(p)
        rules[name] = Rule(name, def_parts)

    for atom in atoms:
        rules[atom] = atom

    for rule in rules.values():
        if not isinstance(rule, Rule):
            continue
        for p in rule.defparts:
            new: List[AnyRule] = []
            for s in p:
                if len(s) > 2 and s[0] == '\'' and s[-1] == '\'':
                    literals.append(s[1:-1])
                    new.append(s[1:-1])
                elif s not in rules:
                    raise RuntimeError(f"Invalid spec: missing rule {s} for {rule.name}")
                else:
                    new.append(rules[s])
            rule.rules.append(new)

    prog = rules['PROGRAM']
    assert isinstance(prog, Rule)
    start_state = Node(*[(prog.name, ('.', *a), ('$',)) for a in prog.rules])


    print(start_state)
    graph = {start_state: start_state}
    start_state.add_children(graph)

    for ix, node in enumerate(graph):
        node.ix = ix
    
    print("Start", start_state.ix, start_state.edges)

    for ix, node in enumerate(graph):
        for token in ('identifier', 'statements', 'fn', '(', ')', '->', '$'):
            shift = False
            reduce = False
            if token in node.edges:
                shift = True
            for r in node.rules:
                if token in r[2]:
                    if r[1][-1] == '.':
                        if reduce or shift:
                            raise RuntimeError("Conflict")
                        reduce = True
                        if r[0] == 'PROGRAM':
                            node.accept = True

    with open('src/compiler/program.txt', 'r') as f:
        data = f.read()

    scanner = ScanTokenizer(data)

    stack = []

    state = start_state

    while True:
        (t, start, end) = scanner.peek_token()
        if t == TOKEN_EMPTY:
            s = '$'
        elif t == TOKEN_STATEMENTS:
            s = 'statements'
        elif t == TOKEN_IDENTIFIER:
            s = 'identifier'
        else:
            s = data[start:end]

        print(s)
        if s in state.edges:
            print("Shift", s)
            print("Push", state.ix)
            stack.append(state)
            state = state.edges[s]
            scanner.get_token()
        else:
            for r in state.rules:
                if s in r[2]:
                    if r[1][-1] == '.':
                        print("Reduce", r[0])
                        for step in r[1][:-1]:
                            state = stack.pop()
                            print("Pop to", state.ix)
                        print("Push", state.ix)
                        stack.append(state)
                        state = state.edges[r[0]]
                        break

            else:
                raise RuntimeError("No action")

        if t == TOKEN_EMPTY:
            break
    if not state.accept:
        raise RuntimeError("Not complete")
    print(state, atoms, literals)


if __name__ == "__main__":
    main()
