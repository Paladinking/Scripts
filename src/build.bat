nasm -fwin64 short.asm
nasm -fwin64 stdasm.asm
cl short.obj stdasm.obj Kernel32.lib /link /NODEFAULTLIB /entry:main