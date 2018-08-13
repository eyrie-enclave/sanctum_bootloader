
#ifndef VM_H
#define VM_H

#include <stdint.h>
#include "csr.h"

void * virt_to_phys(uint64_t va) {
  // Get the page table base pointer
  // Assume Sv39

  // Level 2
  uint64_t * PT2 = (uint64_t*)((read_csr(0x180) & 0xFFFFFFFFFFF) << 12); // SATP: low 44 bits are the PPN
  int vpn2 = (va >> 30) & 0x1FF;
  uint64_t pte2 = PT2[vpn2];
  uint64_t ppn2 = (pte2 >> 10);
  char xwrv2 = pte2 & 0xF;
  if ((xwrv2 & 0x1) == 0) { // Invalid
    return 0;
  } else if (xwrv2 != 1) { // Leaf; giga page
    pte2 |= 0x80;
    PT2[vpn2] = pte2; // TODO: this update must be atomic in general
    uint64_t mask2 = (0xFFFFFFC0000) << 12;
    uint64_t pa = (va & ~(mask2)) | ((ppn2 << 12) & mask2);
    return pa;
  }

  // Level 1
  uint64_t * PT1 = (uint64_t*)(ppn2 << 12);
  int vpn1 = (va >> 21) & 0x1FF;
  uint64_t pte1 = PT1[vpn1];
  uint64_t ppn1 = (pte1 >> 10);
  char xwrv1 = pte1 & 0xF;
  if ((xwrv1 & 0x1) == 0) { // Invalid
    return 0;
  } else if (xwrv1 != 1) { // Leaf; mega page
    pte1 |= 0x80;
    PT1[vpn1] = pte1; // TODO: this update should be atomic
    uint64_t mask1 = (0xFFFFFFFFE00) << 12;
    uint64_t pa = (va & ~(mask1)) | ((ppn1 << 12) & mask1);
    return pa;
  }

  // Level 0
  uint64_t * PT0 = (uint64_t*)(ppn1 << 12);
  int vpn0 = (va >> 12) & 0x1FF;
  uint64_t pte0 = PT0[vpn0];
  uint64_t ppn0 = (pte0 >> 10);
  char xwrv0 = pte0 & 0xF;
  if ((xwrv0 & 0x1) == 0) { // Invalid
    return 0;
  }

  // Set dirty bit
  pte0 |= 0x80;
  PT0[vpn0] = pte0; // TODO: this update should be atomic
  uint64_t mask0 = (0xFFFFFFFFFFF) << 12;
  uint64_t pa = (va & ~(mask0)) | ((ppn0 << 12) & mask0);
  return pa;

  // TODO: probably wise to use MSTATUS to ensure OS permissions are not ignored during SM syscalls
}

#endif VM_H
