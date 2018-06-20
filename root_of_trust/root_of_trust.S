# NUM_CORES should be defined in gcc flag

#include "sanctum_config.h"
#include "encoding.h"

# Requires these symbols:
# DRAM_BASE
# DRAM_END
# MEM_LOADER_BASE
# MSIP_BASE
# NUM_CORES
# M_STACK_BYTES_PER_HART
# BOOT_ADDR


.section .text.rot.entry

.global reset_vector
reset_vector:
    la sp, m_stack_ptr # sp for hart 0 (to be adjusted for other harts)
    csrr a0, mhartid # a0 <-- mhartid

    # 2. All but hart 0 wait for a signal via IPI. Hart 0 proceeds, and will wake other harts.
    bne a0, zero, .hart_non_zero # hart 0

    # 2. Erase DRAM (if applicable)
#ifdef ERASE_DRAM
    li t0, DRAM_BASE
    li t1, DRAM_SIZE
    add t1, t1, t0  # t1 <-- top of DRAM
.dram_erase_loop:
    sd x0, -8(t1)
    sd x0, -16(t1)
    sd x0, -24(t1)
    sd x0, -32(t1)
    sd x0, -40(t1)
    sd x0, -48(t1)
    sd x0, -56(t1)
    sd x0, -64(t1)
    addi t1, t1, -64
    blt t0, t1, .dram_erase_loop
#endif

    # 3. Ask host to load ELF into DRAM
    # request elf from host
    li t0, MEM_LOADER_BASE
    li t1, DRAM_BASE
    sw t1, 0(t0)
    fence # make sure the requet goes out
    # wait for host to finish
.wait_on_elf:
    ld t1, 8(t0)
    bnez t1, .wait_on_elf

    # 4. Hash the bootloader binary in DRAM and compare the hash against an expected constnat
    call rot_hash_and_verify

    # 5. Jump to bootloader binary, if the hash matches an expected value
    beq a0, zero, .rot_bad_hash

    la ra, bootloader_ptr
    jalr ra

    # erase private information on the stack
    la t0, m_stack_base
    la t1, m_stack_ptr
.m_stack_erase_loop:
    sd x0, -8(t1)
    sd x0, -16(t1)
    sd x0, -24(t1)
    sd x0, -32(t1)
    sd x0, -40(t1)
    sd x0, -48(t1)
    sd x0, -56(t1)
    sd x0, -64(t1)
    addi t1, t1, -64
    blt t0, t1, .m_stack_erase_loop

    # wake all ther harts via an ipi
    # t0: value to write to other harts' mip.msip
    li t0, 1
    li t1, MSIP_BASE
    li t2, ((NUM_HARTS - 1) * 4)
.wake_other_harts_loop:
    beqz t2, .done_waking_other_harts
    add t3, t1, t2
    sw t0, 0(t3)
    addi t2, t2, -4
    j .wake_other_harts_loop
    # ---

.done_waking_other_harts:
    # clean private hart state
    call clean_core

    # boot
    la ra, sm_ptr
    jr ra
    # ----

.rot_bad_hash:
    j .rot_bad_hash
    # ----

.hart_non_zero:
    # Adjust stack pointer for this hart
    li t0, M_STACK_SIZE
    mul t0, t0, a0    # offset = mhartid * M_STACK_SIZE
    sub sp, sp, t0    # sp -= offset
    # Enable IPI to wait on
    la t0, .rot_wake_on_ipi
    csrw mtvec, t0
    csrsi mie, 8
    csrsi mstatus, 8
    # Wait for an IPI from core 0
.wait_on_ipi:
    wfi
    j .wait_on_ipi
    # ---

.rot_wake_on_ipi:
    # This hart (not 0) wakes up after an IPI from hart 0
    # clear my mip.msip
    li t0, MSIP_BASE
    csrr t1, mhartid
    slli t1, t1, 2
    add t0, t0, t1
    sw x0, 0(t0)
    fence
    # reset mtvec, mstatus, mie
    csrw mtvec, x0
    csrw mie, x0
    csrw mstatus, x0
    # clean the core
    call clean_core

    # boot
    la ra, sm_ptr
    jr ra
    # ---

# Helper functions

clean_core:
    # 1. Clean all harts state
    # Register File
    # TODO: some of these can be skipped, as they are initialized later
    # x0 is ZERO
    # x1 holds the return address
    la x2, m_stack_ptr # sp for hart 0 (to be adjusted for other harts)
    li x3, 0
    li x4, 0
    li x5, 0
    li x6, 0
    li x7, 0
    li x8, 0
    li x9, 0
    csrr x10, mhartid # a0 <-- mhartid
    la x11, dtb_ptr # a1 <-- &dtb
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
    # FP Register File
    # TODO: is this relevant for our prototype?
    # CSRs
    csrw mscratch, x0
    # TODO: other CSRs to be sanitized?
    jr x1 # return

