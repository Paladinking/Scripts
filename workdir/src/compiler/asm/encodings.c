#include "amd64.h"

const Encoding NOP_ENCODING[] = {
    {
        0, 1,
        {{OPCODE, 0x90}},
        {0},
    }
};

const Encoding MOV_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0x88}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x89}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x89}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x89}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x88}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x89}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x89}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x89}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x8a}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG8, OPERAND_MEM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x8b}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE, 0x8b}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x8b}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 3,
        {{OPCODE, 0xb0}, {PLUS_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 4,
        {{PREFIX, 0x66}, {OPCODE, 0xb8}, {PLUS_REG}, {IMM, 2}},
        {OPERAND_REG16, OPERAND_IMM16},
    },
    {
        2, 3,
        {{OPCODE, 0xb8}, {PLUS_REG}, {IMM, 4}},
        {OPERAND_REG32, OPERAND_IMM32},
    },
    {
        2, 4,
        {{REX, 0x48}, {OPCODE, 0xb8}, {PLUS_REG}, {IMM, 8}},
        {OPERAND_REG64, OPERAND_IMM64},
    },
    {
        2, 4,
        {{OPCODE, 0xc6}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xc7}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 2}},
        {OPERAND_MEM16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0xc7}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc7}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM64, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc7}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG64, OPERAND_IMM32},
    }
};

const Encoding PUSH_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x50}, {PLUS_REG}},
        {OPERAND_REG64},
    }
};

const Encoding POP_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x58}, {PLUS_REG}},
        {OPERAND_REG64},
    }
};

const Encoding RET_ENCODING[] = {
    {
        0, 1,
        {{OPCODE, 0xc3}},
        {0},
    }
};

const Encoding AND_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0x20}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x21}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x21}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x21}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x20}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x21}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x21}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x21}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x22}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG8, OPERAND_MEM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x23}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE, 0x23}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x23}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 2}},
        {OPERAND_MEM16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM64, OPERAND_IMM32},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 2}},
        {OPERAND_REG16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG64, OPERAND_IMM32},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding OR_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0x8}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x9}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x9}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x9}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x8}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x9}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x9}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x9}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0xa}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG8, OPERAND_MEM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xb}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE, 0xb}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xb}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x8}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x8}, {RM_MEM}, {IMM, 2}},
        {OPERAND_MEM16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x8}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x8}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM64, OPERAND_IMM32},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x8}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x8}, {RM_REG}, {IMM, 2}},
        {OPERAND_REG16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x8}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x8}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG64, OPERAND_IMM32},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x3}, {MOD_RM, 0x8}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x3}, {MOD_RM, 0x8}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x3}, {MOD_RM, 0x8}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x3}, {MOD_RM, 0x8}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x3}, {MOD_RM, 0x8}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x3}, {MOD_RM, 0x8}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding SUB_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0x28}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x29}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x29}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x29}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x28}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x29}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x29}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x29}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x2a}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG8, OPERAND_MEM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x2b}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE, 0x2b}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x2b}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 2}},
        {OPERAND_MEM16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM64, OPERAND_IMM32},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 2}},
        {OPERAND_REG16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG64, OPERAND_IMM32},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding ADD_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0x0}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x1}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x1}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x1}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x0}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x1}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x1}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x1}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x2}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG8, OPERAND_MEM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x3}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE, 0x3}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x3}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 2}},
        {OPERAND_MEM16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM64, OPERAND_IMM32},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 2}},
        {OPERAND_REG16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG64, OPERAND_IMM32},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding LEA_ENCODING[] = {
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x8d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x8d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM16},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x8d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x8d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    }
};

const Encoding MUL_ENCODING[] = {
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x20}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 4,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x20}, {RM_MEM}},
        {OPERAND_MEM16},
    },
    {
        1, 3,
        {{OPCODE, 0xf7}, {MOD_RM, 0x20}, {RM_MEM}},
        {OPERAND_MEM32},
    },
    {
        1, 4,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x20}, {RM_MEM}},
        {OPERAND_MEM64},
    },
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x20}, {RM_REG}},
        {OPERAND_REG8},
    },
    {
        1, 4,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x20}, {RM_REG}},
        {OPERAND_REG16},
    },
    {
        1, 3,
        {{OPCODE, 0xf7}, {MOD_RM, 0x20}, {RM_REG}},
        {OPERAND_REG32},
    },
    {
        1, 4,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x20}, {RM_REG}},
        {OPERAND_REG64},
    }
};

const Encoding IMUL_ENCODING[] = {
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x28}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x28}, {RM_REG}},
        {OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0xaf}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0xaf}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xaf}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0xaf}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0xaf}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xaf}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding DIV_ENCODING[] = {
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x30}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 4,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x30}, {RM_MEM}},
        {OPERAND_MEM16},
    },
    {
        1, 3,
        {{OPCODE, 0xf7}, {MOD_RM, 0x30}, {RM_MEM}},
        {OPERAND_MEM32},
    },
    {
        1, 4,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x30}, {RM_MEM}},
        {OPERAND_MEM64},
    },
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x30}, {RM_REG}},
        {OPERAND_REG8},
    },
    {
        1, 4,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x30}, {RM_REG}},
        {OPERAND_REG16},
    },
    {
        1, 3,
        {{OPCODE, 0xf7}, {MOD_RM, 0x30}, {RM_REG}},
        {OPERAND_REG32},
    },
    {
        1, 4,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x30}, {RM_REG}},
        {OPERAND_REG64},
    }
};

const Encoding IDIV_ENCODING[] = {
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x38}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 4,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x38}, {RM_MEM}},
        {OPERAND_MEM16},
    },
    {
        1, 3,
        {{OPCODE, 0xf7}, {MOD_RM, 0x38}, {RM_MEM}},
        {OPERAND_MEM32},
    },
    {
        1, 4,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x38}, {RM_MEM}},
        {OPERAND_MEM64},
    },
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x38}, {RM_REG}},
        {OPERAND_REG8},
    },
    {
        1, 4,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x38}, {RM_REG}},
        {OPERAND_REG16},
    },
    {
        1, 3,
        {{OPCODE, 0xf7}, {MOD_RM, 0x38}, {RM_REG}},
        {OPERAND_REG32},
    },
    {
        1, 4,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x38}, {RM_REG}},
        {OPERAND_REG64},
    }
};

const Encoding XOR_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0x30}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x31}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x31}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x31}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x30}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x31}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x31}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x31}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x32}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG8, OPERAND_MEM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x33}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE, 0x33}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x33}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x30}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x30}, {RM_MEM}, {IMM, 2}},
        {OPERAND_MEM16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x30}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x30}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM64, OPERAND_IMM32},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x30}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x30}, {RM_REG}, {IMM, 2}},
        {OPERAND_REG16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x30}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x30}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG64, OPERAND_IMM32},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x30}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x30}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x30}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x30}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x30}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x30}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding NEG_ENCODING[] = {
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x18}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 4,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x18}, {RM_MEM}},
        {OPERAND_MEM16},
    },
    {
        1, 3,
        {{OPCODE, 0xf7}, {MOD_RM, 0x18}, {RM_MEM}},
        {OPERAND_MEM32},
    },
    {
        1, 4,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x18}, {RM_MEM}},
        {OPERAND_MEM64},
    },
    {
        1, 3,
        {{OPCODE, 0xf6}, {MOD_RM, 0x18}, {RM_REG}},
        {OPERAND_REG8},
    },
    {
        1, 4,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x18}, {RM_REG}},
        {OPERAND_REG16},
    },
    {
        1, 3,
        {{OPCODE, 0xf7}, {MOD_RM, 0x18}, {RM_REG}},
        {OPERAND_REG32},
    },
    {
        1, 4,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x18}, {RM_REG}},
        {OPERAND_REG64},
    }
};

const Encoding SHR_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0xd2}, {MOD_RM, 0x28}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xd3}, {MOD_RM, 0x28}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd3}, {MOD_RM, 0x28}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xd3}, {MOD_RM, 0x28}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM64, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd2}, {MOD_RM, 0x28}, {RM_REG}, {SKIP}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xd3}, {MOD_RM, 0x28}, {RM_REG}, {SKIP}},
        {OPERAND_REG16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd3}, {MOD_RM, 0x28}, {RM_REG}, {SKIP}},
        {OPERAND_REG32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xd3}, {MOD_RM, 0x28}, {RM_REG}, {SKIP}},
        {OPERAND_REG64, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xc0}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xc1}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc1}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc1}, {MOD_RM, 0x28}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc0}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xc1}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc1}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc1}, {MOD_RM, 0x28}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding SAR_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0xd2}, {MOD_RM, 0x38}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xd3}, {MOD_RM, 0x38}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd3}, {MOD_RM, 0x38}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xd3}, {MOD_RM, 0x38}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM64, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd2}, {MOD_RM, 0x38}, {RM_REG}, {SKIP}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xd3}, {MOD_RM, 0x38}, {RM_REG}, {SKIP}},
        {OPERAND_REG16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd3}, {MOD_RM, 0x38}, {RM_REG}, {SKIP}},
        {OPERAND_REG32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xd3}, {MOD_RM, 0x38}, {RM_REG}, {SKIP}},
        {OPERAND_REG64, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xc0}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xc1}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc1}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc1}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc0}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xc1}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc1}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc1}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding SHL_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0xd2}, {MOD_RM, 0x20}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM64, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd2}, {MOD_RM, 0x20}, {RM_REG}, {SKIP}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_REG}, {SKIP}},
        {OPERAND_REG16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_REG}, {SKIP}},
        {OPERAND_REG32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_REG}, {SKIP}},
        {OPERAND_REG64, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xc0}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc0}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding SAL_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0xd2}, {MOD_RM, 0x20}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_MEM}, {SKIP}},
        {OPERAND_MEM64, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd2}, {MOD_RM, 0x20}, {RM_REG}, {SKIP}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_REG}, {SKIP}},
        {OPERAND_REG16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_REG}, {SKIP}},
        {OPERAND_REG32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xd3}, {MOD_RM, 0x20}, {RM_REG}, {SKIP}},
        {OPERAND_REG64, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE, 0xc0}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc0}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xc1}, {MOD_RM, 0x20}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding CALL_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0xe8}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    },
    {
        1, 3,
        {{OPCODE, 0xff}, {MOD_RM, 0x10}, {RM_MEM}},
        {OPERAND_MEM64},
    },
    {
        1, 3,
        {{OPCODE, 0xff}, {MOD_RM, 0x10}, {RM_REG}},
        {OPERAND_REG64},
    }
};

const Encoding JG_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x7f}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x8f}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JA_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x77}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x87}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JLE_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x7e}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x8e}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JBE_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x76}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x86}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JGE_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x7d}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x8d}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JAE_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x73}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x83}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JL_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x7c}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x8c}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JB_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x72}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x82}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JNE_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x75}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x85}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JNZ_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x75}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x85}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JE_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x74}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x84}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JZ_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0x74}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE2, 0x84}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding JMP_ENCODING[] = {
    {
        1, 2,
        {{OPCODE, 0xeb}, {REL_ADDR, 1}},
        {OPERAND_IMM8},
    },
    {
        1, 2,
        {{OPCODE, 0xe9}, {REL_ADDR, 4}},
        {OPERAND_IMM32},
    }
};

const Encoding CMP_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0x38}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x39}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x39}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x39}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x38}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x39}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x39}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x39}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x3a}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG8, OPERAND_MEM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x3b}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE, 0x3b}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x3b}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 2}},
        {OPERAND_MEM16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM64, OPERAND_IMM32},
    },
    {
        2, 4,
        {{OPCODE, 0x80}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x81}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 2}},
        {OPERAND_REG16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0x81}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x81}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG64, OPERAND_IMM32},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x38}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM64, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x83}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG16, OPERAND_IMM8},
    },
    {
        2, 4,
        {{OPCODE, 0x83}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG32, OPERAND_IMM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x83}, {MOD_RM, 0x38}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG64, OPERAND_IMM8},
    }
};

const Encoding TEST_ENCODING[] = {
    {
        2, 4,
        {{OPCODE, 0x84}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x85}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x85}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x85}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0x84}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG8, OPERAND_REG8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0x85}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE, 0x85}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x85}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    },
    {
        2, 4,
        {{OPCODE, 0xf6}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 1}},
        {OPERAND_MEM8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 2}},
        {OPERAND_MEM16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0xf7}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x0}, {RM_MEM}, {IMM, 4}},
        {OPERAND_MEM64, OPERAND_IMM32},
    },
    {
        2, 4,
        {{OPCODE, 0xf6}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 1}},
        {OPERAND_REG8, OPERAND_IMM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE, 0xf7}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 2}},
        {OPERAND_REG16, OPERAND_IMM16},
    },
    {
        2, 4,
        {{OPCODE, 0xf7}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG32, OPERAND_IMM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0xf7}, {MOD_RM, 0x0}, {RM_REG}, {IMM, 4}},
        {OPERAND_REG64, OPERAND_IMM32},
    }
};

const Encoding CMOVG_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x4f}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x4f}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x4f}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x4f}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x4f}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x4f}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVA_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x47}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x47}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x47}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x47}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x47}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x47}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVLE_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x4e}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x4e}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x4e}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x4e}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x4e}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x4e}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVBE_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x46}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x46}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x46}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x46}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x46}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x46}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVGE_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x4d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x4d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x4d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x4d}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x4d}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x4d}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVAE_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x43}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x43}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x43}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x43}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x43}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x43}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVL_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x4c}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x4c}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x4c}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x4c}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x4c}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x4c}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVB_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x42}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x42}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x42}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x42}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x42}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x42}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVNE_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVNZ_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x45}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVE_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding CMOVZ_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0x44}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG64},
    }
};

const Encoding MOVSXD_ENCODING[] = {
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x63}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM32},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE, 0x63}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG32},
    }
};

const Encoding MOVSX_ENCODING[] = {
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xbf}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM16},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xbf}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0xbf}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0xbf}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG16},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0xbe}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM8},
    },
    {
        2, 4,
        {{OPCODE2, 0xbe}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xbe}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0xbe}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE2, 0xbe}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xbe}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG8},
    }
};

const Encoding MOVZX_ENCODING[] = {
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xb7}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM16},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xb7}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG16},
    },
    {
        2, 4,
        {{OPCODE2, 0xb7}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM16},
    },
    {
        2, 4,
        {{OPCODE2, 0xb7}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG16},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0xb6}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG16, OPERAND_MEM8},
    },
    {
        2, 4,
        {{OPCODE2, 0xb6}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xb6}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM8},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0xb6}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG16, OPERAND_REG8},
    },
    {
        2, 4,
        {{OPCODE2, 0xb6}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REG8},
    },
    {
        2, 5,
        {{REX, 0x48}, {OPCODE2, 0xb6}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REG8},
    }
};

const Encoding SETG_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x9f}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x9f}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETA_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x97}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x97}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETLE_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x9e}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x9e}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETBE_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x96}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x96}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETGE_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x9d}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x9d}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETAE_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x93}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x93}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETL_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x9c}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x9c}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETB_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x92}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x92}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETNE_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x95}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x95}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETNZ_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x95}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x95}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETE_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x94}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x94}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding SETZ_ENCODING[] = {
    {
        1, 3,
        {{OPCODE2, 0x94}, {MOD_RM, 0x0}, {RM_MEM}},
        {OPERAND_MEM8},
    },
    {
        1, 3,
        {{OPCODE2, 0x94}, {MOD_RM, 0x0}, {RM_REG}},
        {OPERAND_REG8},
    }
};

const Encoding MOVSS_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x10}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x10}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x11}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM32, OPERAND_REGXMM},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x11}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding MOVSD_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x10}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x10}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x11}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM64, OPERAND_REGXMM},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x11}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding CVTSS2SI_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x2d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x2d}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REGXMM},
    },
    {
        2, 6,
        {{PREFIX, 0xf3}, {REX, 0x48}, {OPCODE2, 0x2d}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REGXMM},
    }
};

const Encoding CVTSD2SI_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x2d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x2d}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REGXMM},
    },
    {
        2, 6,
        {{PREFIX, 0xf2}, {REX, 0x48}, {OPCODE2, 0x2d}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 6,
        {{PREFIX, 0xf2}, {REX, 0x48}, {OPCODE2, 0x2d}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REGXMM},
    }
};

const Encoding CVTSI2SS_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x2a}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x2a}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REG32},
    },
    {
        2, 6,
        {{PREFIX, 0xf3}, {REX, 0x48}, {OPCODE2, 0x2a}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM64},
    },
    {
        2, 6,
        {{PREFIX, 0xf3}, {REX, 0x48}, {OPCODE2, 0x2a}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REG64},
    }
};

const Encoding CVTSI2SD_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x2a}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x2a}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REG32},
    },
    {
        2, 6,
        {{PREFIX, 0xf2}, {REX, 0x48}, {OPCODE2, 0x2a}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM64},
    },
    {
        2, 6,
        {{PREFIX, 0xf2}, {REX, 0x48}, {OPCODE2, 0x2a}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REG64},
    }
};

const Encoding CVTSS2SD_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x5a}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x5a}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding CVTSD2SS_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x5a}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x5a}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding XORPS_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x57}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM128},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x57}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding ADSS_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x58}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x58}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding ADSD_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x58}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x58}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding SUBSS_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x5c}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x5c}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding SUBSD_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x5c}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x5c}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding MULSS_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x59}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x59}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding MULSD_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x59}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x59}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding DIVSS_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x5e}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x5e}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding DIVSD_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x5e}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x5e}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding MOVDQA_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x6f}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REGXMM, OPERAND_MEM128},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x6f}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x7f}, {MOD_RM, 0x0}, {RM_MEM}, {REG_REG}},
        {OPERAND_MEM128, OPERAND_REGXMM},
    },
    {
        2, 5,
        {{PREFIX, 0x66}, {OPCODE2, 0x7f}, {MOD_RM, 0x0}, {RM_REG}, {REG_REG}},
        {OPERAND_REGXMM, OPERAND_REGXMM},
    }
};

const Encoding CVTTSS2SI_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x2c}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM32},
    },
    {
        2, 5,
        {{PREFIX, 0xf3}, {OPCODE2, 0x2c}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REGXMM},
    },
    {
        2, 6,
        {{PREFIX, 0xf3}, {REX, 0x48}, {OPCODE2, 0x2c}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REGXMM},
    }
};

const Encoding CVTTSD2SI_ENCODING[] = {
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x2c}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG32, OPERAND_MEM64},
    },
    {
        2, 5,
        {{PREFIX, 0xf2}, {OPCODE2, 0x2c}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG32, OPERAND_REGXMM},
    },
    {
        2, 6,
        {{PREFIX, 0xf2}, {REX, 0x48}, {OPCODE2, 0x2c}, {MOD_RM, 0x0}, {REG_REG}, {RM_MEM}},
        {OPERAND_REG64, OPERAND_MEM64},
    },
    {
        2, 6,
        {{PREFIX, 0xf2}, {REX, 0x48}, {OPCODE2, 0x2c}, {MOD_RM, 0x0}, {REG_REG}, {RM_REG}},
        {OPERAND_REG64, OPERAND_REGXMM},
    }
};

const Encodings ENCODINGS[] = {
    {1, NOP_ENCODING}, //0
    {21, MOV_ENCODING}, //1
    {1, PUSH_ENCODING}, //2
    {1, POP_ENCODING}, //3
    {1, RET_ENCODING}, //4
    {26, AND_ENCODING}, //5
    {26, OR_ENCODING}, //6
    {26, SUB_ENCODING}, //7
    {26, ADD_ENCODING}, //8
    {4, LEA_ENCODING}, //9
    {8, MUL_ENCODING}, //10
    {8, IMUL_ENCODING}, //11
    {8, DIV_ENCODING}, //12
    {8, IDIV_ENCODING}, //13
    {26, XOR_ENCODING}, //14
    {8, NEG_ENCODING}, //15
    {16, SHR_ENCODING}, //16
    {16, SAR_ENCODING}, //17
    {16, SHL_ENCODING}, //18
    {16, SAL_ENCODING}, //19
    {3, CALL_ENCODING}, //20
    {2, JG_ENCODING}, //21
    {2, JA_ENCODING}, //22
    {2, JLE_ENCODING}, //23
    {2, JBE_ENCODING}, //24
    {2, JGE_ENCODING}, //25
    {2, JAE_ENCODING}, //26
    {2, JL_ENCODING}, //27
    {2, JB_ENCODING}, //28
    {2, JNE_ENCODING}, //29
    {2, JNZ_ENCODING}, //30
    {2, JE_ENCODING}, //31
    {2, JZ_ENCODING}, //32
    {2, JMP_ENCODING}, //33
    {26, CMP_ENCODING}, //34
    {16, TEST_ENCODING}, //35
    {6, CMOVG_ENCODING}, //36
    {6, CMOVA_ENCODING}, //37
    {6, CMOVLE_ENCODING}, //38
    {6, CMOVBE_ENCODING}, //39
    {6, CMOVGE_ENCODING}, //40
    {6, CMOVAE_ENCODING}, //41
    {6, CMOVL_ENCODING}, //42
    {6, CMOVB_ENCODING}, //43
    {6, CMOVNE_ENCODING}, //44
    {6, CMOVNZ_ENCODING}, //45
    {6, CMOVE_ENCODING}, //46
    {6, CMOVZ_ENCODING}, //47
    {2, MOVSXD_ENCODING}, //48
    {10, MOVSX_ENCODING}, //49
    {10, MOVZX_ENCODING}, //50
    {2, SETG_ENCODING}, //51
    {2, SETA_ENCODING}, //52
    {2, SETLE_ENCODING}, //53
    {2, SETBE_ENCODING}, //54
    {2, SETGE_ENCODING}, //55
    {2, SETAE_ENCODING}, //56
    {2, SETL_ENCODING}, //57
    {2, SETB_ENCODING}, //58
    {2, SETNE_ENCODING}, //59
    {2, SETNZ_ENCODING}, //60
    {2, SETE_ENCODING}, //61
    {2, SETZ_ENCODING}, //62
    {4, MOVSS_ENCODING}, //63
    {4, MOVSD_ENCODING}, //64
    {3, CVTSS2SI_ENCODING}, //65
    {4, CVTSD2SI_ENCODING}, //66
    {4, CVTSI2SS_ENCODING}, //67
    {4, CVTSI2SD_ENCODING}, //68
    {2, CVTSS2SD_ENCODING}, //69
    {2, CVTSD2SS_ENCODING}, //70
    {2, XORPS_ENCODING}, //71
    {2, ADSS_ENCODING}, //72
    {2, ADSD_ENCODING}, //73
    {2, SUBSS_ENCODING}, //74
    {2, SUBSD_ENCODING}, //75
    {2, MULSS_ENCODING}, //76
    {2, MULSD_ENCODING}, //77
    {2, DIVSS_ENCODING}, //78
    {2, DIVSD_ENCODING}, //79
    {4, MOVDQA_ENCODING}, //80
    {3, CVTTSS2SI_ENCODING}, //81
    {4, CVTTSD2SI_ENCODING} //82
};
