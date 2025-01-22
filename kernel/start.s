.section .text
.global _start
_entry:
    la sp, stack0
    li a0, 4096
    add sp, sp, a0
    call main
spin:
    j spin