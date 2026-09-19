# stub.s - entry point of the packed payload (see stub.c). The build system
# generates payload_len.s with the decompressed size and embeds XenoShell.lz.
.global _start
_start:
	lis 1, 0x817F
	b stub_main

.section .rodata
.balign 4
.global payload_lz
payload_lz:
	.incbin "build/XenoShell.lz"
.global payload_lz_end
payload_lz_end:
