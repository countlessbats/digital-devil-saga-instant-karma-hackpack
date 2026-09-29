/* Mod code layout in free EE RAM. Code/rodata are written by the pnach every frame;
 * bss lives in a NOLOAD region the pnach never touches, so state persists (RAM there starts zeroed). */
ENTRY(set_logic)
SECTIONS
{
  .bss 0x000F0800 (NOLOAD) : { *(.sbss .sbss.* .bss .bss.* COMMON) }
  ASSERT(. <= 0x000F1000, "bss overflows into code")
  .text 0x000F1000 : { *(.text .text.*) *(.rodata .rodata.* .srodata .srodata.*) }
  ASSERT(. <= 0x000FF000, "code too large")
  .data : { *(.data .data.* .sdata .sdata.*) }
  ASSERT(SIZEOF(.data) == 0, "initialized .data is not supported; use const or bss")
  /DISCARD/ : { *(.got .MIPS.* .reginfo .pdr .comment .note* .gnu* .eh_frame* .debug*) }
}
