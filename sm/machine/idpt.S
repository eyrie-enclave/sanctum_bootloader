#include "encoding.h"

  .section ".idpt","a",@progbits
  .align RISCV_PGSHIFT

  .globl _idpt_start, _idpt_end
_idpt_start:
  .incbin "../idpt.bin"
_idpt_end:
