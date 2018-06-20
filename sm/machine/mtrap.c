#include "mtrap.h"
#include "mcall.h"
#include "htif.h"
#include "atomic.h"
#include "bits.h"
#include "vm.h"
#include "uart.h"
#include "fdt.h"
#include "unprivileged_memory.h"
#include "platform/platform_interface.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "enclave_syscall.h"

void __attribute__((noreturn)) bad_trap(uintptr_t* regs, uintptr_t dummy, uintptr_t mepc)
{
  die("machine mode: unhandlable trap %d @ %p", read_csr(mcause), mepc);
}

void redirect_trap(uintptr_t epc, uintptr_t mstatus, uintptr_t mbadaddr)
{
  write_csr(sbadaddr, mbadaddr);
  write_csr(sepc, epc);
  write_csr(scause, read_csr(mcause));
  write_csr(mepc, read_csr(stvec));

  uintptr_t new_mstatus = mstatus & ~(MSTATUS_SPP | MSTATUS_SPIE | MSTATUS_SIE);
  uintptr_t mpp_s = MSTATUS_MPP & (MSTATUS_MPP >> 1);
  new_mstatus |= (mstatus * (MSTATUS_SPIE / MSTATUS_SIE)) & MSTATUS_SPIE;
  new_mstatus |= (mstatus / (mpp_s / MSTATUS_SPP)) & MSTATUS_SPP;
  write_csr(mstatus, new_mstatus);

  extern void __redirect_trap();
  return __redirect_trap();
}

// Specific handlers for S-mode traps
// ==================================

static uintptr_t mcall_console_putchar(uint8_t ch)
{
  if (uart) {
    uart_putchar(ch);
  } else if (platform__use_htif()) {
    htif_console_putchar(ch);
  }
  return 0;
}

void poweroff()
{
  if (platform__use_htif()) {
    htif_poweroff();
  } else {
    while (1);
  }
}

void putstring(const char* s)
{
  while (*s)
    mcall_console_putchar(*s++);
}

void vprintm(const char* s, va_list vl)
{
  char buf[256];
  vsnprintf(buf, sizeof buf, s, vl);
  putstring(buf);
}

void printm(const char* s, ...)
{
  va_list vl;

  va_start(vl, s);
  vprintm(s, vl);
  va_end(vl);
}

static void send_ipi(uintptr_t recipient, int event)
{
  if (((platform__disabled_hart_mask >> recipient) & 1)) return;
  atomic_or(&OTHER_HLS(recipient)->mipi_pending, event);
  mb();
  *OTHER_HLS(recipient)->ipi = 1;
}

static uintptr_t mcall_console_getchar()
{
  if (uart) {
    return uart_getchar();
  } else if (platform__use_htif()) {
    return htif_console_getchar();
  } else {
    return '\0';
  }
}

static uintptr_t mcall_clear_ipi()
{
  return clear_csr(mip, MIP_SSIP) & MIP_SSIP;
}

static uintptr_t mcall_shutdown()
{
  poweroff();
}

static uintptr_t mcall_set_timer(uint64_t when)
{
  *HLS()->timecmp = when;
  clear_csr(mip, MIP_STIP);
  set_csr(mie, MIP_MTIP);
  return 0;
}

static void send_ipi_many(uintptr_t* pmask, int event)
{
  _Static_assert(NUM_HARTS <= 8 * sizeof(*pmask), "# harts > uintptr_t bits");
  uintptr_t mask = hart_mask;
  if (pmask)
    mask &= load_uintptr_t(pmask, read_csr(mepc));

  // send IPIs to everyone
  for (uintptr_t i = 0, m = mask; m; i++, m >>= 1)
    if (m & 1)
      send_ipi(i, event);

  if (event == IPI_SOFT)
    return;

  // wait until all events have been handled.
  // prevent deadlock by consuming incoming IPIs.
  uint32_t incoming_ipi = 0;
  for (uintptr_t i = 0, m = mask; m; i++, m >>= 1)
    if (m & 1)
      while (*OTHER_HLS(i)->ipi)
        incoming_ipi |= atomic_swap(HLS()->ipi, 0);

  // if we got an IPI, restore it; it will be taken after returning
  if (incoming_ipi) {
    *HLS()->ipi = incoming_ipi;
    mb();
  }
}

// Route S-mode traps
// ==================

void ecall_from_s_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  write_csr(mepc, mepc + 4);

  uintptr_t n = regs[17], arg0 = regs[10], /* arg1 = regs[11], */ retval, ipi_type;

  switch (n)
  {
    // SM calls from OS Kernel
    // Standard OS calls
    case SBI_CONSOLE_PUTCHAR:
      retval = mcall_console_putchar(arg0);
      break;
    case SBI_CONSOLE_GETCHAR:
      retval = mcall_console_getchar();
      break;
    case SBI_SEND_IPI:
      ipi_type = IPI_SOFT;
      goto send_ipi;
    case SBI_REMOTE_SFENCE_VMA:
    case SBI_REMOTE_SFENCE_VMA_ASID:
      ipi_type = IPI_SFENCE_VMA;
      goto send_ipi;
    case SBI_REMOTE_FENCE_I:
      ipi_type = IPI_FENCE_I;
send_ipi:
      send_ipi_many((uintptr_t*)arg0, ipi_type);
      retval = 0;
      break;
    case SBI_CLEAR_IPI:
      retval = mcall_clear_ipi();
      break;
    case SBI_SHUTDOWN:
      retval = mcall_shutdown();
      break;
    case SBI_SET_TIMER:
#if __riscv_xlen == 32
      retval = mcall_set_timer(arg0 + ((uint64_t)arg1 << 32));
#else
      retval = mcall_set_timer(arg0);
#endif
      break;
    default:
      retval = -ENOSYS;
      break;
  }
  regs[10] = retval;
}

// Specific handlers for U-mode traps
// ==================================


// Route U-mode traps
// ==================

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

void ecall_from_u_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  //putstring("SM CALL FROM U\n");

  write_csr(mepc, mepc + 4);

  uintptr_t n = regs[17], arg0 = regs[10], arg1 = regs[11], arg2 = regs[12], arg3 = regs[13], retval, ipi_type;

  switch (n)
  {
    // Security monitor calls
    case UBI_SM_DEADBEEF:
      retval = 0xDEADBEEF;
      break;
    case UBI_SM_GET_FIELD:
      retval = sm_fetch_field( (void*)virt_to_phys(arg0), (uint64_t)arg1 );
      break;
    case UBI_SM_AES:
      retval = sm_aes_cbc( (void*)virt_to_phys(arg0), (uint8_t*)virt_to_phys(arg1), (uint32_t)arg2 );
      break;
    case UBI_SM_SIGN:
      retval = sm_sign_message( (uint8_t*)virt_to_phys(arg0), (void*)virt_to_phys(arg1), (uint32_t)arg2 );
      break;
    case UBI_SM_POET:
      retval = sm_poet( (uint8_t*)virt_to_phys(arg0), (uint8_t*)virt_to_phys(arg1), (uint8_t*)virt_to_phys(arg2), (uint32_t)arg3 );
      break;
    default:
      retval = -ENOSYS;
      break;
  }
  regs[10] = retval;
}

void pmp_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  redirect_trap(mepc, read_csr(mstatus), read_csr(mbadaddr));
}

static void machine_page_fault(uintptr_t* regs, uintptr_t dummy, uintptr_t mepc)
{
  // MPRV=1 iff this trap occurred while emulating an instruction on behalf
  // of a lower privilege level. In that case, a2=epc and a3=mstatus.
  if (read_csr(mstatus) & MSTATUS_MPRV) {
    return redirect_trap(regs[12], regs[13], read_csr(mbadaddr));
  }
  bad_trap(regs, dummy, mepc);
}

void trap_from_machine_mode(uintptr_t* regs, uintptr_t dummy, uintptr_t mepc)
{
  uintptr_t mcause = read_csr(mcause);

  switch (mcause)
  {
    case CAUSE_LOAD_PAGE_FAULT:
    case CAUSE_STORE_PAGE_FAULT:
    case CAUSE_FETCH_ACCESS:
    case CAUSE_LOAD_ACCESS:
    case CAUSE_STORE_ACCESS:
      return machine_page_fault(regs, dummy, mepc);
    default:
      bad_trap(regs, dummy, mepc);
  }
}

void delegate_trap(uintptr_t* regs, uintptr_t dummy, uintptr_t mepc)
{
  // Send this trap down to S-mode
  redirect_trap(mepc, read_csr(mstatus), read_csr(mbadaddr));
}

