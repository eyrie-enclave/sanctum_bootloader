// See LICENSE for license details.

reset:
  # If this is not core 0:
  csrr t0, mhartid
  bnez t0, other_core

  # invariant: mhartid=0
  # set up a stack
  li sp, 0x84000000 # somewhere deep in DRAM

  # run the bootloader
  call bootloader

  # clean memory, after the fact
  call clean_memory

  # clean core state, after the fact
  call clean_core

  # boot payload
	csrr a0, mhartid
	#la a1, _dtb
  la t0, sanctum_sm_ptr
  jr t0

  # <end of bootloader>

## Booting other cores
other_core:
  ### clean other core state
  call clean_core

  ### allow MSIP interrupts
  li a2, 8
  csrw mie, a2

  ### spin until MSIP is set
.other_core_wfi_loop:
  wfi
  csrr a2, mip
  andi a2, a2, 8
  beqz a2, .other_core_wfi_loop

  ## init other core
  nop
  la ra, .other_core_wfi_loop
  jr ra

## Helpful subroutines
clean_core:
  # arguments:
  # - x0 = ra : return address
  # li x1, 0 # x1 = ra is the return address
  li x2, 0
  li x3, 0
  li x4, 0
  li x5, 0x1000 # PC at start
  li x6, 0
  li x7, 0
  li x8, 0
  li x9, 0
  csrr   a0, mhartid
  addi   a1, t0, 32 # addi   a1, t0, &dtb, where t0 = 1000
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
  jr ra

clean_memory:
  # arguments:
  # - x0 = ra : return address
  # - : start address
  #
  fence
  jr ra
