#include <machine/mtrap.h>
#include <machine/bits.h>
#include <machine/mcall.h>
#include <encoding.h>

# Reset event (happens after reset, measurement, etc)
# ==================================================
  .option norvc
  .section .text.init,"ax",@progbits
  .globl reset_vector
reset_vector:
  j do_reset

  .text
  .globl do_reset
do_reset:
  li x1, 0
  li x2, 0 // sp: stack pointer
  li x3, 0
  li x4, 0
  li x5, 0
  li x6, 0
  li x7, 0
  li x8, 0
  li x9, 0
// save a0 and a1; arguments from previous boot loader stage:
//  li x10, 0
//  li x11, 0
  li x12, 0
  li x13, 0
  li x14, 0
  li x15, 0
  li x16, 0
  li x17, 0
  li x18, 0
  li x19, 0
  li x20, 0
  li x21, 0
  li x22, 0
  li x23, 0
  li x24, 0
  li x25, 0
  li x26, 0
  li x27, 0
  li x28, 0
  li x29, 0
  li x30, 0
  li x31, 0
  csrw mscratch, x0

  # Set Sanctum CSRs to prevent immediate problems
  # TODO: this is not necessary in a complete SM
  li t0, -1
  csrw 0x7c0, t0  # Sanctum MEVBASE
  csrw 0x7c3, t0  # Sanctum MMRBM

  # write mtvec and make sure it sticks
  la t0, trap_vector
  csrw mtvec, t0
  csrr t1, mtvec
1:bne t0, t1, 1b

  # Reserve 1 page per hart for machine mode stack
  # MENTRY_FRAME_SIZE at the top

  la sp, m_stack_ptr
  li a2, MENTRY_FRAME_SIZE
  sub sp, sp, a2
  csrr a3, mhartid
  slli a2, a3, RISCV_PGSHIFT
  sub sp, sp, a2

  # Boot on the first unmasked hart
  la a4, platform__disabled_hart_mask
  LOAD a4, 0(a4)
  addi a5, a4, 1
  not a4, a4
  and a4, a4, a5
  srl a4, a4, a3
  andi a4, a4, 1
  bnez a4, init_first_hart

  # set MSIE bit to receive IPI
  li a2, MIP_MSIP
  csrw mie, a2

.LmultiHart:
#if MAX_HARTS > 1
  # wait for an IPI to signal that it's safe to boot
  wfi

  # masked harts never start
  la a4, platform__disabled_hart_mask
  LOAD a4, 0(a4)
  srl a4, a4, a3
  andi a4, a4, 1
  bnez a4, .LmultiHart

  # only start if mip is set
  csrr a2, mip
  andi a2, a2, MIP_MSIP
  beqz a2, .LmultiHart

  # make sure our hart id is within a valid range
  fence
  li a2, NUM_HARTS
  bltu a3, a2, init_other_hart
#endif
  wfi
  j .LmultiHart
