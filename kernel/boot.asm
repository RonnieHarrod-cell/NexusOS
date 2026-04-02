section .bss
alignb 16
stack resb 16384
stack_top:

section .text
global _start
extern kmain

_start:
    lea rsp, [stack_top]
    call kmain

.hang:
    hlt
    jmp .hang
