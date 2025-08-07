from typing import Tuple, Union, List
import sys

class Opcode:
    def __init__(self, byte: int) -> None:
        self.byte = byte
    def __str__(self) -> str:
        return f'{{OPCODE, {hex(self.byte)}}}'

class Opcode2:
    def __init__(self, byte: int) -> None:
        self.byte = byte
    def __str__(self) -> str:
        return f'{{OPCODE2, {hex(self.byte)}}}'

class Prefix:
    def __init__(self, byte: int) -> None:
        self.byte = byte
    def __str__(self) -> str:
        return f'{{PREFIX, {hex(self.byte)}}}'

class ModRm:
    def __init__(self, byte: int) -> None:
        self.byte = byte
    def __str__(self) -> str:
        return f'{{MOD_RM, {hex(self.byte)}}}'

def ModRmOpcode(n: int) -> ModRm:
    return ModRm(n << 3)

class Rex:
    def __init__(self, byte: int) -> None:
        self.byte = byte
    def __str__(self) -> str:
        return f'{{REX, {hex(self.byte)}}}'

class RegReg:
    def __str__(self) -> str:
        return f'{{REG_REG}}'

class PlusReg:
    def __str__(self) -> str:
        return f'{{PLUS_REG}}'

class RmMem:
    def __str__(self) -> str:
        return f'{{RM_MEM}}'

class RmReg:
    def __str__(self) -> str:
        return f'{{RM_REG}}'

class Imm:
    def __init__(self, byte: int) -> None:
        self.byte = byte
    def __str__(self) -> str:
        return f'{{IMM, {self.byte}}}'

class RelAddr:
    def __init__(self, byte: int) -> None:
        self.byte = byte
    def __str__(self) -> str:
        return f'{{REL_ADDR, {self.byte}}}'

class Skip:
    def __str__(self) -> str:
        return "{SKIP}"

class RmMemAndReg:
    def __str__(self) -> str:
        return f"PLACEHOLDER RmMemAndReg"

class SizesPrefix:
    def __str__(self) -> str:
        return f"PLACEHOLDER SizesPrefix"

class VarImm64:
    def with_size(self, size: int) -> Imm:
        return Imm(size)

    def __str__(self) -> str:
        return "PLACEHOLDER VarImm64"

class VarImm32:
    def with_size(self, size: int) -> Imm:
        if size == 8:
            return Imm(4)
        return Imm(size)

    def __str__(self) -> str:
        return "PLACEHOLDER VarImm32"

Mnemonic = Union[Prefix, Rex, Opcode, Opcode2, ModRm, RegReg, PlusReg, RmMem, RmReg,
                 Imm, RelAddr, Skip, RmMemAndReg, SizesPrefix, VarImm32, VarImm64]

class OperandRegOrMemCls:
    def with_size(self, size: int) -> 'Operand':
        if size == 1:
            return OperandRegOrMem8()
        if size == 2:
            return OperandRegOrMem16()
        if size == 4:
            return OperandRegOrMem32()
        if size == 8:
            return OperandRegOrMem64()
        raise RuntimeError('Bad Size')

    def reg(self) -> 'Operand':
        return OperandRegCls()

    def mem(self) -> 'Operand':
        return OperandMemCls()

OperandRegOrMem = OperandRegOrMemCls()

class OperandRegOrMem8:
    def reg(self) -> str:
        return OPERAND_REG8
    def mem(self) -> str:
        return OPERAND_MEM8

class OperandRegOrMem16:
    def reg(self) -> str:
        return OPERAND_REG16
    def mem(self) -> str:
        return OPERAND_MEM16

class OperandRegOrMem32:
    def reg(self) -> str:
        return OPERAND_REG32
    def mem(self) -> str:
        return OPERAND_MEM32

class OperandRegOrMem64:
    def reg(self) -> str:
        return OPERAND_REG64
    def mem(self) -> str:
        return OPERAND_MEM64

class OperandRegCls:
    def with_size(self, size: int) -> str:
        if size == 1:
            return OPERAND_REG8
        if size == 2:
            return OPERAND_REG16
        if size == 4:
            return OPERAND_REG32
        if size == 8:
            return OPERAND_REG64
        raise RuntimeError("Bad size")

OperandReg = OperandRegCls()

class OperandMemCls:
    def with_size(self, size: int) -> str:
        if size == 1:
            return OPERAND_MEM8
        if size == 2:
            return OPERAND_MEM16
        if size == 4:
            return OPERAND_MEM32
        if size == 8:
            return OPERAND_MEM64
        raise RuntimeError("Bad size")

OperandMem = OperandMemCls()

class OperandImm32Cls:
    def with_size(self, size: int) -> str:
        if size == 1:
            return OPERAND_IMM8
        if size == 2:
            return OPERAND_IMM16
        if size == 4:
            return OPERAND_IMM32
        if size == 8:
            return OPERAND_IMM32
        raise RuntimeError("Bad size")

OperandImm32 = OperandImm32Cls()

class OperandImm64Cls:
    def with_size(self, size: int) -> str:
        if size == 1:
            return OPERAND_IMM8
        if size == 2:
            return OPERAND_IMM16
        if size == 4:
            return OPERAND_IMM32
        if size == 8:
            return OPERAND_IMM64
        raise RuntimeError("Bad size")

OperandImm64 = OperandImm64Cls()

OPERAND_MEM8 = "OPERAND_MEM8"
OPERAND_MEM16 = "OPERAND_MEM16"
OPERAND_MEM32 = "OPERAND_MEM32"
OPERAND_MEM64 = "OPERAND_MEM64"
OPERAND_REG8 = "OPERAND_REG8"
OPERAND_REG16 = "OPERAND_REG16"
OPERAND_REG32 = "OPERAND_REG32"
OPERAND_REG64 = "OPERAND_REG64"
OPERAND_IMM8 = "OPERAND_IMM8"
OPERAND_IMM16 = "OPERAND_IMM16"
OPERAND_IMM32 = "OPERAND_IMM32"
OPERAND_IMM64 = "OPERAND_IMM64"

Operand = Union[str, OperandRegOrMemCls, OperandRegCls, OperandMemCls, OperandImm32Cls,
                OperandImm64Cls, OperandRegOrMem8, OperandRegOrMem16, OperandRegOrMem32, 
                OperandRegOrMem64]

def split_size(ops: Tuple[Operand, ...], mems: Tuple[Mnemonic, ...],
               include_byte: bool=True) -> List[Tuple[Tuple[Operand, ...],
                                                      Tuple[Mnemonic, ...]]]:
    for ix, m in enumerate(mems):
        if isinstance(m, SizesPrefix):
            mw = mems[:ix] + (Prefix(0x66),) + mems[ix + 1:]
            mw: Tuple[Mnemonic, ...] = tuple(
                m.with_size(2) if hasattr(m, 'with_size') else m for m in mw #type: ignore
            )
            ow: Tuple[Operand, ...] = tuple(
                o.with_size(2) if hasattr(o, 'with_size') else o for o in ops #type: ignore
            )
            md = mems[:ix] + mems[ix + 1:]
            md: Tuple[Mnemonic, ...] = tuple(
                m.with_size(4) if hasattr(m, 'with_size') else m for m in md #type: ignore
            )
            od: Tuple[Operand, ...] = tuple(
                o.with_size(4) if hasattr(o, 'with_size') else o for o in ops #type: ignore
            )
            mq = mems[:ix] + (Rex(0x48),) + mems[ix + 1:]
            mq: Tuple[Mnemonic, ...] = tuple(
                m.with_size(8) if hasattr(m, 'with_size') else m for m in mq #type: ignore
            )
            oq: Tuple[Operand, ...] = tuple(
                o.with_size(8) if hasattr(o, 'with_size') else o for o in ops #type: ignore
            )
            if not include_byte:
                return [(ow, mw), (od, md), (oq, mq)]
            mb = mems[:ix] + mems[ix + 1:]
            mb: Tuple[Mnemonic, ...] = tuple(
                m.with_size(1) if hasattr(m, 'with_size') else m for m in mb #type: ignore
            )
            for mb_ix, m in enumerate(mb):
                if isinstance(m, Opcode):
                    mb = mb[:mb_ix] + (Opcode(m.byte - 1),) + mb[mb_ix + 1:]
                    break
            else:
                raise RuntimeError("Opcode missing for Byte size")
            ob: Tuple[Operand, ...] = tuple(
                o.with_size(1) if hasattr(o, 'with_size') else o for o in ops #type: ignore
            )
            return [(ob, mb), (ow, mw), (od, md), (oq, mq)]


    return [(ops, mems)]

def split_encoding(ops: Tuple[Operand, ...], mems: Tuple[Mnemonic, ...],
                   include_byte: bool=True) -> List[Tuple[Tuple[Operand, ...],
                                                          Tuple[Mnemonic, ...]]]:
    for ix, m in enumerate(mems):
        if isinstance(m, RmMemAndReg):
            m1 = mems[:ix] + (RmMem(),) + mems[ix + 1:]
            o1: Tuple[Operand, ...] = tuple(
                o.mem() if hasattr(o, 'mem') else o for o in ops #type: ignore
            )

            m2 = mems[:ix] + (RmReg(),) + mems[ix + 1:]
            o2: Tuple[Operand, ...] = tuple(
                o.reg() if hasattr(o, 'reg') else o for o in ops #type: ignore
            )
            return [*split_size(o1, m1, include_byte), *split_size(o2, m2, include_byte)]

    return split_size(ops, mems, include_byte)

encodings = []

def output(name: str, data: List[Tuple[Tuple[Operand, ...],
                                       Tuple[Mnemonic, ...]]]) -> None:
    count = len(data)
    parts = []
    for ops, mems in data:
        ops_str = [s for s in ops if isinstance(s, str)]
        assert len(ops_str) == len(ops)
            
        s = '    {\n'
        s += '        ' + str(len(ops)) + ', ' + str(len(mems)) + ',\n'
        s += '        {' + ', '.join([str(m) for m in mems]) + '},\n'
        if len(ops) == 0:
            s += '        {0},\n'
        else:
            s += '        {' + ', '.join(ops_str) + '},\n'
        s += '    }'
        parts.append(s)

    s = f'const Encoding {name}_ENCODING[] = {{\n' + ',\n'.join(parts) + '\n};'
    encodings.append((name, s, count))


def standard_op(opcode_mr: int, opcode_rm: int,
                opcode_mi: int, opcode_mi8: int,
                modrm_opcode: int) -> List[Tuple[Tuple[Operand, ...],
                                                 Tuple[Mnemonic, ...]]]:
    v = split_encoding((OperandRegOrMem, OperandReg),
                       (SizesPrefix(), Opcode(opcode_mr),
                        ModRm(0), RmMemAndReg(), RegReg()))
    v += split_encoding((OperandReg, OperandMem),
                        (SizesPrefix(), Opcode(opcode_rm), ModRm(0),
                         RegReg(), RmMem()))
    v += split_encoding((OperandRegOrMem, OperandImm32),
                        (SizesPrefix(), Opcode(opcode_mi), ModRmOpcode(modrm_opcode),
                         RmMemAndReg(), VarImm32()))
    v += split_encoding((OperandRegOrMem, OPERAND_IMM8),
                        (SizesPrefix(), Opcode(opcode_mi8), ModRmOpcode(modrm_opcode), 
                         RmMemAndReg(), Imm(1)),
                        include_byte=False)
    return v

def main() -> None:
    nop = split_encoding((), (Opcode(0x90),))
    output('NOP', nop)

    mov = split_encoding((OperandRegOrMem, OperandReg),
                         (SizesPrefix(), Opcode(0x89), ModRm(0), RmMemAndReg(), RegReg()))
    mov += split_encoding((OperandReg, OperandMem),
                          (SizesPrefix(), Opcode(0x8B), ModRm(0), RegReg(), RmMem()))
    mov += [((OPERAND_REG8, OPERAND_IMM8),
            (Opcode(0xB0), PlusReg(), Imm(1)))]
    mov += split_encoding((OperandReg, OperandImm64),
                          (SizesPrefix(), Opcode(0xB8), PlusReg(), VarImm64()),
                          include_byte=False)
    mov += split_encoding((OperandMem, OperandImm32),
                          (SizesPrefix(), Opcode(0xC7), ModRmOpcode(0), RmMem(), VarImm32()))
    mov += [((OPERAND_REG64, OPERAND_IMM32), 
             (Rex(0x48), Opcode(0xC7), ModRmOpcode(0), RmReg(), Imm(4)))]
    output('MOV', mov)

    push = split_encoding((OPERAND_REG64,), (Opcode(0x50), PlusReg()))
    output('PUSH', push)

    pop = split_encoding((OPERAND_REG64,), (Opcode(0x58), PlusReg()))
    output('POP', pop)

    ret = split_encoding((), (Opcode(0xC3),))
    output('RET', ret)

    and_ = standard_op(0x21, 0x23, 0x81, 0x83, 4)
    output('AND', and_)

    or_ = standard_op(0x09, 0x0b, 0x81, 0x03, 1)
    output('OR', or_)

    sub = standard_op(0x29, 0x2B, 0x81, 0x83, 5)
    output('SUB', sub)

    add = standard_op(0x1, 0x3, 0x81, 0x83, 0)
    output('ADD', add)

    lea = split_encoding((OPERAND_REG64, OPERAND_MEM8),
                         (Rex(0x48), Opcode(0x8D), ModRm(0), RegReg(), RmMem()))
    lea += split_encoding((OPERAND_REG64, OPERAND_MEM16),
                         (Rex(0x48), Opcode(0x8D), ModRm(0), RegReg(), RmMem()))
    lea += split_encoding((OPERAND_REG64, OPERAND_MEM32),
                         (Rex(0x48), Opcode(0x8D), ModRm(0), RegReg(), RmMem()))
    lea += split_encoding((OPERAND_REG64, OPERAND_MEM64),
                         (Rex(0x48), Opcode(0x8D), ModRm(0), RegReg(), RmMem()))
    output('LEA', lea)

    mul = split_encoding((OperandRegOrMem,),
                         (SizesPrefix(), Opcode(0xf7), ModRmOpcode(4), RmMemAndReg()))
    output("MUL", mul)

    imul = split_encoding((OperandRegOrMem8(),), 
                          (Opcode(0xf6), ModRmOpcode(5), RmMemAndReg())) 
    imul += split_encoding((OperandReg, OperandRegOrMem),
                          (SizesPrefix(), Opcode2(0xAF), ModRm(0), RegReg(), RmMemAndReg()),
                           include_byte=False)
    output("IMUL", imul)

    div = split_encoding((OperandRegOrMem,),
                         (SizesPrefix(), Opcode(0xf7), ModRmOpcode(6), RmMemAndReg()))
    output("DIV", div)

    idiv = split_encoding((OperandRegOrMem,),
                         (SizesPrefix(), Opcode(0xf7), ModRmOpcode(7), RmMemAndReg()))
    output("IDIV", idiv)

    xor = standard_op(0x31, 0x33, 0x81, 0x83, 6)
    output("XOR", xor)

    neg = split_encoding((OperandRegOrMem,),
                         (SizesPrefix(), Opcode(0xf7), ModRmOpcode(3), RmMemAndReg()))
    output("NEG", neg)

    for op, modrm_op in [('SHR', 5), ('SAR', 7), ('SHL', 4), ('SAL', 4)]:
        v = split_encoding((OperandRegOrMem, OPERAND_REG8),
                             (SizesPrefix(), Opcode(0xd3), ModRmOpcode(modrm_op), 
                              RmMemAndReg(), Skip()))
        v += split_encoding((OperandRegOrMem, OPERAND_IMM8),
                              (SizesPrefix(), Opcode(0xc1), ModRmOpcode(modrm_op),
                               RmMemAndReg(), Imm(1)))
        output(op, v)

    call = split_encoding((OPERAND_IMM32,), (Opcode(0xE8), RelAddr(4)))
    call += split_encoding((OperandRegOrMem64(),),
                           (Opcode(0xFF), ModRmOpcode(2), RmMemAndReg()))
    output('CALL', call)

    for (name, o1, o2) in [('JG', 0x7F, 0x8F), ('JA', 0x77, 0x87), ('JLE', 0x7E, 0x8E),
                           ('JBE', 0x76, 0x86), ('JGE', 0x7D, 0x8D), ('JAE', 0x73, 0x83),
                           ('JL', 0x7C, 0x8C), ('JB', 0x72, 0x82), ('JNE', 0x75, 0x85),
                           ('JNZ', 0x75, 0x85), ('JE', 0x74, 0x84), ('JZ', 0x74, 0x84)]:
        v = split_encoding((OPERAND_IMM8,), (Opcode(o1), RelAddr(1)))
        v += split_encoding((OPERAND_IMM16,), (Prefix(0x66), Opcode2(o2), RelAddr(2)))
        v += split_encoding((OPERAND_IMM32,), (Opcode2(o2), RelAddr(4)))
        output(name, v)

    jmp = split_encoding((OPERAND_IMM8,), (Opcode(0xEB), RelAddr(1)))
    jmp += split_encoding((OPERAND_IMM16,), (Prefix(0x66), Opcode(0xE9), RelAddr(2)))
    jmp += split_encoding((OPERAND_IMM32,), (Opcode(0xE9), RelAddr(4)))
    output('JMP', jmp)

    cmp = standard_op(0x39, 0x3B, 0x81, 0x83, 7)
    output('CMP', cmp)

    test = split_encoding((OperandRegOrMem, OperandReg),
                          (SizesPrefix(), Opcode(0x85), ModRm(0), RmMemAndReg(), RegReg()))
    test += split_encoding((OperandRegOrMem, OperandImm32),
                           (SizesPrefix(), Opcode(0xF7), ModRmOpcode(0),
                            RmMemAndReg(), VarImm32()))
    output('TEST', test)

    for (name, o) in [('CMOVG', 0x4f), ('CMOVA', 0x47), ('CMOVLE', 0x4E), ('CMOVBE', 0x46),
                      ('CMOVGE', 0x4D), ('CMOVAE', 0x43), ('CMOVL', 0x4C), ('CMOVB', 0x42),
                      ('CMOVNE', 0x45), ('CMOVNZ', 0x45), ('CMOVE', 0x44), ('CMOVZ', 0x44)]:
        v = split_encoding((OperandReg, OperandRegOrMem),
                           (SizesPrefix(), Opcode2(o), ModRm(0), RegReg(), RmMemAndReg()),
                           include_byte=False)
        output(name, v)

    movsxd = split_encoding((OPERAND_REG64, OperandRegOrMem32()),
                            (Rex(0x48), Opcode(0x63), ModRm(0), RegReg(), RmMemAndReg()))
    output('MOVSXD', movsxd)

    movsx = split_encoding((OPERAND_REG64, OperandRegOrMem16()),
                           (Rex(0x48), Opcode2(0xBF), ModRm(0), RegReg(), RmMemAndReg()))
    movsx += split_encoding((OPERAND_REG32, OperandRegOrMem16()),
                            (Opcode2(0xBF), ModRm(0), RegReg(), RmMemAndReg()))
    movsx += split_encoding((OperandReg, OperandRegOrMem8()),
                            (SizesPrefix(), Opcode2(0xBE), ModRm(0), RegReg(), RmMemAndReg()),
                            include_byte=False)
    output('MOVSX', movsx)

    movzx = split_encoding((OPERAND_REG64, OperandRegOrMem16()),
                           (Rex(0x48), Opcode2(0xB7), ModRm(0), RegReg(), RmMemAndReg()))
    movzx += split_encoding((OPERAND_REG32, OperandRegOrMem16()),
                            (Opcode2(0xB7), ModRm(0), RegReg(), RmMemAndReg()))
    movzx += split_encoding((OperandReg, OperandRegOrMem8()),
                            (SizesPrefix(), Opcode2(0xB6), ModRm(0), RegReg(), RmMemAndReg()),
                            include_byte=False)
    output('MOVZX', movzx)

    for (name, o) in [('SETG', 0x9f), ('SETA', 0x97), ('SETLE', 0x9e), ('SETBE', 0x96),
                      ('SETGE', 0x9d), ('SETAE', 0x93), ('SETL', 0x9c), ('SETB', 0x92),
                      ('SETNE', 0x95), ('SETNZ', 0x95), ('SETE', 0x94), ('SETZ', 0x94)]:
        setcc = split_encoding((OperandRegOrMem8(),), 
                               (Opcode2(o), ModRmOpcode(0), RmMemAndReg()))
        output(name, setcc)

    s = '\n\n'.join([val for name, val, count in encodings])
    parts = [f'    {{{count}, {name}_ENCODING}}{"," if ix + 1 < len(encodings) else ""} //{ix}' 
             for ix, (name, val, count) in enumerate(encodings)]

    if len(sys.argv) < 2:
        print("#include \"amd64.h\"\n")
        print(s, end='\n\n')
        print('const Encodings ENCODINGS[] = {');
        print('\n'.join(parts))
        print('};')
    else:
        with open(sys.argv[1], 'w') as file:
            print("#include \"amd64.h\"\n", file=file)
            print(s, end='\n\n', file=file)
            print('const Encodings ENCODINGS[] = {', file=file);
            print('\n'.join(parts), file=file)
            print('};', file=file)

if __name__ == "__main__":
    main()
