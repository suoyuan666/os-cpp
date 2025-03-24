# exec(init, argv)
.globl start
start:
        la a0, init
        la a1, argv
        li a7, 6
        ecall

# for(;;) exit();
exit:
        li a7, 1
        ecall
        jal exit

# char init[] = "/bin/init\0";
init:
  .string "/bin/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .quad init
  .quad 0
