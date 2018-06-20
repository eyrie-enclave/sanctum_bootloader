#include "sanctum_config.h"
#include "encoding.h"

  .section ".mstacks"
  .globl m_stack_base, m_stack_ptr
  .align RISCV_PGSHIFT
m_stack_base:
  .skip RISCV_PGSIZE * NUM_HARTS
m_stack_ptr:
