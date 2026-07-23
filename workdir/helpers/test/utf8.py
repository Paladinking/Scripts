print("len")
for i in range(0, 0xFF + 1):
    if i in range(0xC2, 0xDF + 1):
        print(2, end=", ")
    elif i == 0xE0:
        print(3, end=", ")
    elif i in range(0xE1, 0xEC + 1):
        print(3, end=", ")
    elif i == 0xED:
        print(3, end=", ")
    elif i in range(0xEE, 0xEF + 1):
        print(3, end=", ")
    elif i == 0xF0:
        print(4, end=", ")
    elif i in range(0xF1, 0xF3 + 1):
        print(4, end=", ")
    elif i == 0xF4:
        print(4, end=", ")
    else:
        print(1, end=", ")


print("Valid")
for i in range(0x7F + 1, 0xFF + 1):
    if i in range(0xC2, 0xDF + 1):
        print(hex(0x80), end=", ")
    elif i == 0xE0:
        print(hex(0xA0), end=", ")
    elif i in range(0xE1, 0xEC + 1):
        print(hex(0x80), end=", ")
    elif i == 0xED:
        print(hex(0x9F), end=", ")
    elif i in range(0xEE, 0xEF + 1):
        print(hex(0x80), end=", ")
    elif i == 0xF0:
        print(hex(0x90), end=", ")
    elif i in range(0xF1, 0xF3 + 1):
        print(hex(0x80), end=", ")
    elif i == 0xF4:
        print(hex(0x8F), end=", ")
    else:
        print(hex(0x0), end=", ")

    #print(hex(i))
