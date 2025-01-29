.section .text
.global _start
_start:
    la sp, stack0
    li a0, 4096
    add sp, sp, a0
    call start
spin:
    j spin
