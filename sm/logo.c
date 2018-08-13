#include <stdio.h>
#include "machine/mtrap.h"
#include "platform/platform_interface.h"
#include "randomart.h"
#include "csr.h"
#include "sanctum_config.h"


extern uint8_t SM_H[64];
extern uint8_t PK_SM[32];

void print_logo()
{
  char str[256];
  putstring("MIT SANCTUM SECURITY MONITOR\n");
  putstring("============================\n");
  snprintf(str, sizeof(str), "  A Sanctum processor with up to %d harts and 0x%x Bytes of DRAM (%d MB).\n", NUM_HARTS, DRAM_SIZE, (unsigned)(DRAM_SIZE)/1024/1024);
  putstring(str);
  snprintf(str, sizeof(str), "  This processor implements %d protection domains, each of size 0x%x Bytes. (%d MB)\n", XLEN, REGION_SIZE, (unsigned)(REGION_SIZE)/1024/1024);
  putstring(str);
  putstring("  This security monitor's hash is \n\n");
  randomart(SM_H, 64, str);
  putstring("+--[ SHA3  512 ]--+\n");
  putstring(str);
  putstring("\n");
  putstring("  This security monitor's public key (PK_SM) is: \n\n");
  randomart(PK_SM, 32, str);
  putstring("+--[ED25519 256]--+\n");
  putstring(str);

  putstring("\nSanctum CSRs at boot:\n");
  putstring("=====================\n");
  snprintf(str, sizeof(str), "  CSR MEVBASE:   0x%lx\n", read_csr(0x7c0));
  putstring(str);
  snprintf(str, sizeof(str), "  CSR MEVMASK:   0x%lx\n", read_csr(0x7c1));
  putstring(str);
  snprintf(str, sizeof(str), "  CSR MEATP:     0x%lx\n", read_csr(0x7c2));
  putstring(str);
  snprintf(str, sizeof(str), "  CSR MMRBM:     0x%lx\n", read_csr(0x7c3));
  putstring(str);
  snprintf(str, sizeof(str), "  CSR MEMRBM:    0x%lx\n", read_csr(0x7c4));
  putstring(str);
  snprintf(str, sizeof(str), "  CSR MPARBASE:  0x%lx\n", read_csr(0x7c5));
  putstring(str);
  snprintf(str, sizeof(str), "  CSR MPARMASK:  0x%lx\n", read_csr(0x7c6));
  putstring(str);
  snprintf(str, sizeof(str), "  CSR MEPARBASE: 0x%lx\n", read_csr(0x7c7));
  putstring(str);
  snprintf(str, sizeof(str), "  CSR MEPARMASK: 0x%lx\n", read_csr(0x7c8));
  putstring(str);


  putstring("\nHave a nice day!\n");
  putstring("============================\n");
}
