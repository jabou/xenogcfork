ENTRY(_start)
SECTIONS
{
  . = 0x81700000;
  .text : { *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) *(.bss*) *(.sbss*) *(.data*) }
}
