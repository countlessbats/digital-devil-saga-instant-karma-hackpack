/* Mod code layout in free EE RAM (0x95000-0x100000 is unused by the game).
 *   0xF0000-0xF0FFF  hand-written asm (turbo, test injection)
 *   0xF1000-0xF7FFF  C code + rodata (written by the pnach every frame)
 *   0xF8000-0xFDFFF  C bss, NOLOAD: never written by the pnach, so state persists (RAM starts zeroed)
 *   0xFF000-0xFFFFF  live-tunable tables (layout) */
ENTRY(set_logic)
SECTIONS
{
  .bss 0x000F8000 (NOLOAD) : { *(.sbss .sbss.* .bss .bss.* COMMON) }
  ASSERT(. <= 0x000FE000, "bss too large")
  .text 0x000F1000 : { *(.text .text.*) *(.rodata .rodata.* .srodata .srodata.*) }
  ASSERT(. <= 0x000F8000, "code too large")
  .data : { *(.data .data.* .sdata .sdata.*) }
  ASSERT(SIZEOF(.data) == 0, "initialized .data is not supported; use const or bss")
  /DISCARD/ : { *(.got .MIPS.* .reginfo .pdr .comment .note* .gnu* .eh_frame* .debug*) }
}
