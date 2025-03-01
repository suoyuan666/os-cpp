OUTPUT_ARCH( "riscv" )

SECTIONS
{
  . = 0x0;

  .text : {
    *(.text .text.*)
    *(.srodata .srodata.*)
    *(.rodata .rodata.*)
  }

  /* .rodata : { */
  /*   . = ALIGN(16); */
  /*   *(.srodata .srodata.*) */
  /*   . = ALIGN(16); */
  /*   *(.rodata .rodata.*) */
  /*  } : text */
  /**/
  . = ALIGN(0x1000);
  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*)
    . = ALIGN(16);
    *(.data .data.*)
  }

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*)
    . = ALIGN(16);
    *(.bss .bss.*)
  }

  PROVIDE(end = .);
}
