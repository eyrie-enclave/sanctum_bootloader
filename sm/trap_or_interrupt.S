// See LICENSE for license details.

#include "machine/mtrap.h"
#include "machine/bits.h"
#include "encoding.h"

#define TRAP_FROM_MACHINE_MODE_VECTOR 13
#define BAD_TRAP_VECTOR 0

  .data
  .align 6
enclave_trap_table:
  # NOTE: only RA and T0 are saved at this point
  .word .enclave_aex            //  0: misaligned instruction address
  .word .enclave_aex            //  1: instruction access fault
  .word .enclave_aex            //  2: illegal instruction
  .word .enclave_aex            //  3: breakpoint
  .word .enclave_aex            //  4: misaligned load addr
  .word .enclave_aex            //  5: load access fault
  .word .enclave_aex            //  6: misaligned store/AMO addr
  .word .enclave_aex            //  7: store/AMO access fault
  .word .enclave_ecall_from_u_trap  //  8: ECALL from U-mode
  .word .enclave_aex            //  9: ECALL from S-mode
  .word .enclave_aex            // 10: (reserved)
  .word .enclave_aex            // 11: ECALL from M-mode
  .word .enclave_aex            // 12: instruction page fault
  .word .enclave_aex            // 13: load page fault
  .word .enclave_aex            // 14: (reserved)
  .word .enclave_aex            // 15: store/AMO page fault

  .data
  .align 6
enclave_interrupt_table:
  # NOTE: only RA and T0 are saved at this point
  .word .enclave_aex            //  0: user software interrupt
  .word .enclave_aex            //  1: supervisor software interrupt
  .word .enclave_aex            //  2: (reserved)
  .word .enclave_aex            //  3: machine softwre interrupt
  .word .enclave_aex            //  4: user timer interrupt
  .word .enclave_aex            //  5: supervisor timer interrupt
  .word .enclave_aex            //  6: (reserved)
  .word .enclave_aex            //  7: machine timer interrupt
  .word .enclave_aex            //  8: user external interrupt
  .word .enclave_aex            //  9: supervisor external interrupt
  .word .enclave_aex            // 10: (reserved)
  .word .enclave_aex            // 11: machine external interrupt

  .data
  .align 6
os_trap_table:
  # NOTE: only RA and T0 are saved at this point
  .word .delegate_to_s          //  0: misaligned instruction address
  .word .pmp_trap               //  1: instruction access fault
  .word .illegal_insn_trap      //  2: illegal instruction
  .word .delegate_to_s          //  3: breakpoint
  .word .misaligned_load_trap   //  4: misaligned load addr
  .word .pmp_trap               //  5: load access fault
  .word .misaligned_store_trap  //  6: misaligned store/AMO addr
  .word .pmp_trap               //  7: store/AMO access fault
  .word .bad_event              //  8: ECALL from U-mode
  .word .ecall_from_s_trap      //  9: ECALL from S-mode
  .word .bad_event              // 10: (reserved)
  .word .bad_event              // 11: ECALL from M-mode
  .word .delegate_to_s          // 12: instruction page fault
  .word .delegate_to_s          // 13: load page fault
  .word .bad_event              // 14: (reserved)
  .word .delegate_to_s          // 15: store/AMO page fault

  .data
  .align 6
os_interrupt_table:
  # NOTE: only RA and T0 are saved at this point
  .word .delegate_to_s          //  0: user software interrupt
  .word .delegate_to_s          //  1: supervisor software interrupt
  .word .bad_event              //  2: (reserved)
  .word .handle_mipi            //  3: machine softwre interrupt
  .word .delegate_to_s          //  4: user timer interrupt
  .word .delegate_to_s          //  5: supervisor timer interrupt
  .word .bad_event              //  6: (reserved)
  .word .handle_mtimer_interrupt //  7: machine timer interrupt
  .word .bad_event              //  8: user external interrupt
  .word .bad_event              //  9: supervisor external interrupt
  .word .bad_event              // 10: (reserved)
  .word .bad_event              // 11: machine external interrupt

# Traps / Interrupts
# ==================
  .text
  .globl trap_vector
trap_vector:
  # 1. Prepare to route the event: set up an M-mode stack, make a0, a1 usable.
  csrrw sp, mscratch, sp # sp <--> mscratch (save SP, restore M-mode SP)
  beqz sp, .Ltrap_from_machine_mode

  STORE ra, 1*REGBYTES(sp)
  STORE t0, 5*REGBYTES(sp)
  csrr t0, mcause
  csrr ra, 0x7c4 #memrbm

  # Route OS/Enclave, Trap/Interrupta
  # Is this an enclave event?
  beqz ra, .Lroute_os_trap_or_interrupt
  # -- Yes! Enclave event.
  # -- Is this enclave event an interrupt?
  bgez t0, .Lroute_enclave_trap
  # -- -- Yes! Enclave interrupt.
  la ra, enclave_interrupt_table
  j .Lroute_all_events

  # -- -- No, this is an enclave trap
.Lroute_enclave_trap:
  la ra, enclave_trap_table
  j .Lroute_all_events

  # -- No, this is an OS event
.Lroute_os_trap_or_interrupt:
  # -- Is this OS event an interrupt?
  bgez t0, .Lroute_os_trap
  # -- -- Yes! OS interrupt
  la ra, os_interrupt_table
  j .Lroute_all_events

  # -- -- No, this is an OS trap
.Lroute_os_trap:
  la ra, os_trap_table

.Lroute_all_events:
  sll t0, t0, 2
  add t0, ra, t0
  lwu ra, 0(t0) # load address of handler from event table
  jr ra # route event
  # (no return)

.Ltrap_from_machine_mode:
  csrr sp, mscratch
  addi sp, sp, -INTEGER_CONTEXT_SIZE
  STORE a0,10*REGBYTES(sp)
  STORE a1,11*REGBYTES(sp)
  li a1, TRAP_FROM_MACHINE_MODE_VECTOR
  j .Lroute_os_trap

# Calling convention helpers
# ==========================

.save_regs_and_set_args:
  # Preserve the registers.
  STORE x0, (sp) # Zero x0's save slot.
  #STORE ra, 1*REGBYTES(sp) # previously saved
  #STORE sp, 2*REGBYTES(sp) # separately handled
  STORE gp, 3*REGBYTES(sp)
  STORE tp, 4*REGBYTES(sp)
  # STORE t0, 5*REGBYTES(sp) # previously saved
  STORE t1, 6*REGBYTES(sp)
  STORE t2, 7*REGBYTES(sp)
  STORE s0, 8*REGBYTES(sp)
  STORE s1, 9*REGBYTES(sp)
  STORE a0,10*REGBYTES(sp)
  STORE a1,11*REGBYTES(sp)
  STORE a2,12*REGBYTES(sp)
  STORE a3,13*REGBYTES(sp)
  STORE a4,14*REGBYTES(sp)
  STORE a5,15*REGBYTES(sp)
  STORE a6,16*REGBYTES(sp)
  STORE a7,17*REGBYTES(sp)
  STORE s2,18*REGBYTES(sp)
  STORE s3,19*REGBYTES(sp)
  STORE s4,20*REGBYTES(sp)
  STORE s5,21*REGBYTES(sp)
  STORE s6,22*REGBYTES(sp)
  STORE s7,23*REGBYTES(sp)
  STORE s8,24*REGBYTES(sp)
  STORE s9,25*REGBYTES(sp)
  STORE s10,26*REGBYTES(sp)
  STORE s11,27*REGBYTES(sp)
  STORE t3,28*REGBYTES(sp)
  STORE t4,29*REGBYTES(sp)
  STORE t5,30*REGBYTES(sp)
  STORE t6,31*REGBYTES(sp)
  # Handler arguments: (uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
  mv a0, sp        # a0 <- uintptr_t* regs
  csrr a1, mcause  # a1 <- uintptr_t  mcause
  csrr a2, mepc    # a0 <- uintptr_t  mepc
  ret

.restore_regs:
  # Restore all of the registers.
  # x0 need not be restored
  #LOAD ra, 1*REGBYTES(sp) # loaded later
  #LOAD t0, 2*REGBYTES(sp) # separately hadled
  LOAD gp, 3*REGBYTES(sp)
  LOAD tp, 4*REGBYTES(sp)
  # LOAD t0, 5*REGBYTES(sp) # loaded later
  LOAD t1, 6*REGBYTES(sp)
  LOAD t2, 7*REGBYTES(sp)
  LOAD s0, 8*REGBYTES(sp)
  LOAD s1, 9*REGBYTES(sp)
  LOAD a0,10*REGBYTES(sp)
  LOAD a1,11*REGBYTES(sp)
  LOAD a2,12*REGBYTES(sp)
  LOAD a3,13*REGBYTES(sp)
  LOAD a4,14*REGBYTES(sp)
  LOAD a5,15*REGBYTES(sp)
  LOAD a6,16*REGBYTES(sp)
  LOAD a7,17*REGBYTES(sp)
  LOAD s2,18*REGBYTES(sp)
  LOAD s3,19*REGBYTES(sp)
  LOAD s4,20*REGBYTES(sp)
  LOAD s5,21*REGBYTES(sp)
  LOAD s6,22*REGBYTES(sp)
  LOAD s7,23*REGBYTES(sp)
  LOAD s8,24*REGBYTES(sp)
  LOAD s9,25*REGBYTES(sp)
  LOAD s10,26*REGBYTES(sp)
  LOAD s11,27*REGBYTES(sp)
  LOAD t3,28*REGBYTES(sp)
  LOAD t4,29*REGBYTES(sp)
  LOAD t5,30*REGBYTES(sp)
  LOAD t6,31*REGBYTES(sp)
  ret

# Event handlers
# ==============


# Generic enclave events
.enclave_aex:
  call .save_regs_and_set_args
  call enclave_aex
  call .restore_regs
  j .delegate_to_s

.enclave_ecall_from_u_trap:
  call .save_regs_and_set_args
  call ecall_from_enclave_trap
  call .restore_regs
  j .perform_mret

# Generic OS events
.delegate_to_s:
  # delegate trap/interrupt to S mode
  # Set sIE to 0
  STORE t1, 6*REGBYTES(sp)
  STORE t2, 7*REGBYTES(sp)
  # Get MSTATUS
  csrr t0, mstatus
  # MSTATUS_SPP:    0x00000100
  # MSTATUS_SPIE:   0x00000020
  # MSTATUS_SIE:    0x00000002
  # MSTATUS_MPP:    0x00001800
  # sPIE <-- sIE
  andi t1, t0, 0x2
  sll t2, t1, 4
  # sPP <-- mPP[0]
  srl t1, t0, 3
  andi t1, t1, 0x100
  or t2, t2, t1
  # mPP <-- 0x1
  li t1, 0x0800
  or t2, t2, t1
  # Set sIE to 0, add sPP, sPIE
  li t1, 0xFFFFFFFFFFFFE6DD
  and t0, t0, t1
  or t0, t0, t2
  csrw mstatus, t0
  # sepc <-- mepc
  csrr t0, mepc
  csrw sepc, t0
  # scause <-- mcause
  csrr t0, mcause
  csrw scause, t0
  # sbadaddr <-- mbadaddr
  csrr t0, mbadaddr
  csrw sbadaddr, t0
  # Set M-mode return address to S-mode handler
  # mepc <-- stvec
  csrr t0, stvec
  csrw mepc, t0
  # Restore dirty registers and stack pointer
  LOAD t1, 6*REGBYTES(sp)
  LOAD t2, 7*REGBYTES(sp)
  # (continue to perform_mret)

.perform_mret:
  # Go back whence we came.
  LOAD ra, 1*REGBYTES(sp)
  LOAD t0, 5*REGBYTES(sp)
  csrrw sp, mscratch, sp
  mret

.bad_event:
  li t0, BAD_TRAP_VECTOR
  j .Lroute_os_trap
  # j .bad_event

# OS Traps
.pmp_trap:
  call .save_regs_and_set_args
  call pmp_trap
  call .restore_regs
  j .perform_mret

.illegal_insn_trap:
  call .save_regs_and_set_args
  call illegal_insn_trap
  call .restore_regs
  j .perform_mret

.misaligned_load_trap:
  call .save_regs_and_set_args
  call misaligned_load_trap
  call .restore_regs
  j .perform_mret

.misaligned_store_trap:
  call .save_regs_and_set_args
  call misaligned_store_trap
  call .restore_regs
  j .perform_mret

.ecall_from_s_trap:
  call .save_regs_and_set_args
  call ecall_from_s_trap
  call .restore_regs
  j .perform_mret

# OS Interrupts
.handle_mipi:
  LOAD t0, MENTRY_IPI_OFFSET(sp)
  sw x0, (t0)
  fence
# Now, decode the cause(s).
  STORE t1, 6*REGBYTES(sp)
  addi t0, sp, MENTRY_IPI_PENDING_OFFSET
  amoswap.w a0, x0, (a0)
  and t1, t0, IPI_SOFT
  beqz t1, 1f
  csrs mip, MIP_SSIP
1:
  andi t1, t0, IPI_FENCE_I
  beqz t1, 1f
  fence.i
1:
  andi t1, t0, IPI_SFENCE_VMA
  beqz t1, 1f
  sfence.vma
1:
  LOAD t1, 6*REGBYTES(sp)
  j .perform_mret

.handle_mtimer_interrupt:
  #Simply clear MTIE and raise STIP.
  li t0, MIP_MTIP
  csrc mie, t0
  li t0, MIP_STIP
  csrs mip, t0
  j .perform_mret

# Relics from the past
# ====================

  .globl __redirect_trap
__redirect_trap:
  # reset sp to top of M-mode stack
  li t0, MACHINE_STACK_SIZE
  add sp, sp, t0
  neg t0, t0
  and sp, sp, t0
  addi sp, sp, -MENTRY_FRAME_SIZE
  j restore_mscratch

restore_mscratch:
  # Restore mscratch, so future traps will know they didn't come from M-mode.
  csrw mscratch, sp
  call .restore_regs
  j .perform_mret
