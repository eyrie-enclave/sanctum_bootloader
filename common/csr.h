#ifndef CSR_H
#define CSR_H

#define CSR_TRNG        0xCC0
#define CSR_MPUFSELECT  0x7CB
#define CSR_MPUFDISABLE 0x7CC
#define CSR_MPUFREADOUT 0xFC0

#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

#define write_csr(reg, val) ({ \
  asm volatile ("csrw " #reg ", %0" :: "rK"(val)); })

#define swap_csr(reg, val) ({ unsigned long __tmp; \
  asm volatile ("csrrw %0, " #reg ", %1" : "=r"(__tmp) : "rK"(val)); \
  __tmp; })

#endif
