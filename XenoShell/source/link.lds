ENTRY(_start)
SECTIONS
{
  . = 0x81500000;
  .text : { *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data : { *(.data .data.*) *(.sdata .sdata.*) }
  . = ALIGN(32);
  __bss_start = .;
  .bss : { *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) }
  . = ALIGN(4);
  __bss_end = .;
  _end = .;
  /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
