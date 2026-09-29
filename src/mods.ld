/* Mod memory map (0x95000-0x100000 is unused by the game):
 *   0xA0000-0xBFFFF  Prey Eyes icon atlas (texture blob, written by the pnach)
 *   0xC0000-0xDFFFF  C code + rodata (written by the pnach)
 *   0xE0000-0xEEFFF  C bss, NOLOAD: never written by the pnach, so state persists (RAM starts zeroed)
 *   0xEF000-0xEFFFF  Prey Eyes knowledge table
 *   0xF0000-0xF0FFF  hand-written asm (turbo, test injection)
 *   0xFD000-0xFDFFF  Prey Eyes tunables;  0xFE000 test trace;  0xFF000 SET layout tunables */
ENTRY(set_logic)
SECTIONS
{
  .bss 0x000E0000 (NOLOAD) : { *(.sbss .sbss.* .bss .bss.* COMMON) }
  ASSERT(. <= 0x000EF000, "bss too large")
  .text 0x000C0000 : { *(.text .text.*) *(.rodata .rodata.* .srodata .srodata.*) }
  ASSERT(. <= 0x000E0000, "code too large")
  .data : { *(.data .data.* .sdata .sdata.*) }
  ASSERT(SIZEOF(.data) == 0, "initialized .data is not supported; use const or bss")
  /DISCARD/ : { *(.got .MIPS.* .reginfo .pdr .comment .note* .gnu* .eh_frame* .debug*) }
}
