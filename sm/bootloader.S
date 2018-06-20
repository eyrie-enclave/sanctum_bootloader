  .section ".bootloader","a",@progbits

  .globl _bootloader_start, _bootloader_end
_bootloader_start:
  .incbin "../bootloader.bin"
_bootloader_end:
