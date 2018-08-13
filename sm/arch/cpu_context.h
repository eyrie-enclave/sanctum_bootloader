#ifndef BARE_CPU_CONTEXT_H_INCLUDED
#define BARE_CPU_CONTEXT_H_INCLUDED

#include "base_types.h"
#include "csr.h"

// Obtains the number of cores installed in the system.
//
// The implementation may be very slow, so the return value should be cached.
//
// The implementation may use privileged instructions.
static inline size_t read_core_count() {
  return 1; // NOTE: hard-wried to 1 core at the moment
}

// Cores are numbered starting from 0.
static inline size_t current_core() {
  return read_csr(mhartid);
}

// Flush all TLBs on the current core.
//
// This does not flush any cache.
static inline void flush_tlbs() {
  asm volatile ("sfence.vma");
}

// Flush all the caches belonging to the current core.
// This includes branch structures.
//
// This does not flush TLBs, and does not flush the shared last-level cache.
void flush_private_caches() {
  // TODO: Flush L1 I$
  // TODO: Flush L1 D$
  // TODO: Flush branch structures
}

// Sets the core's cache index shift.
//
// This must be set to identical values on all cores. Undefined behavior will
// occur otherwise.
//
// This can only be issued by the security monitor.
static inline void set_cache_index_shift(size_t cache_index_shift) {
  // Nope, this has no effect - the address rotation is staticaly configured by the hardware in this SM
}

// Sets the EPTBR (enclave page table base register).
//
// This can only be issued by the security monitor. An invalid DRAM address
// will lock up or reboot the machine.
static inline void set_eptbr(uintptr_t value) {
  // Low 44+16 bits are value, high 4 bits are MODE=8: Sv39
  write_csr(0x7c2, (value&(-1L>>4) | (8<<60))); // MEATP
}

// Sets the PTBR (page table base register).
//
// This can only be issued by the security monitor. An invalid DRAM address
// will lock up or reboot the machine.
static inline void set_ptbr(uintptr_t value) {
  // Low 44+16 bits are value, high 4 bits are MODE=8: Sv39
  write_csr(satp, (value&(-1L>>4) | (8<<60))); // MEATP
}

// Sets the EPARBASE (enclave protected address range base) register.
//
// This can only be issued by the security monitor.
static inline void set_epar_base(uintptr_t value) {
  write_csr(0x7c7, value); // E PAR BASE
}

// Sets the PARBASE (protected address range base) register.
//
// This can only be issued by the security monitor.
static inline void set_par_base(uintptr_t value) {
  write_csr(0x7c5, value); // PAR BASE
}

// Sets the EPARMASK (enclave protected address range mask) register.
//
// This can only be issued by the security monitor.
static inline void set_epar_mask(uintptr_t value) {
  write_csr(0x7c8, value); // E PAR MASK
}

// Sets the PARMASK (protected address range mask) register.
//
// This can only be issued by the security monitor.
static inline void set_par_mask(uintptr_t value) {
  write_csr(0x7c6, value); // PAR MASK
}

// Sets the EVBASE (enclave virtual address base register).
//
// This can only be issued by the security monitor.
static inline void set_ev_base(uintptr_t value) {
  write_csr(0x7c0, value); // EV BASE
}

// Sets the EVMASK (enclave virtual address mask register).
//
// This can only be issued by the security monitor.
static inline void set_ev_mask(uintptr_t value) {
  write_csr(0x7c1, value); // EV MASK
}

// Loads the DRBMAP (DRAM region bitmap) register from memory.
static inline void set_drb_map(uintptr_t phys_addr) {
  write_csr(0x7c3, *(size_t*)phys_addr); // RBM
}

// Loads the EDRBMAP (enclave DRAM region bitmap) register from memory.
static inline void set_edrb_map(uintptr_t phys_addr) {
  write_csr(0x7c4, *(size_t*)phys_addr); // ERBM
}

// The execution context saved when the monitor is invoked.
//
// The structure is guaranteed to have the following members:
// pc - program counter (RIP on x86)
// sp - stack pointer
typedef struct {
  uintptr_t pc;       // x1
  uintptr_t stack;    // x14
  uintptr_t gprs[29]; // x2-13, x15-x31
} exec_state_t;

#endif  // !defined(BARE_CPU_CONTEXT_H_INCLUDED)
