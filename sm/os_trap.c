#include <machine/mcall.h>
#include <machine/mtrap.h>
#include <machine/atomic.h>
#include <machine/bits.h>
#include <machine/fdt.h>
#include <machine/vm.h>
#include <machine/unprivileged_memory.h>
#include <devices/htif.h>
#include <devices/uart.h>
#include <platform/platform_interface.h>
#include <public/api.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

void __attribute__((noreturn)) bad_trap(uintptr_t* regs, uintptr_t dummy, uintptr_t mepc)
{
  die("machine mode: unhandlable trap %d @ %p", read_csr(mcause), mepc);
}

void redirect_trap_to_s(uintptr_t epc, uintptr_t mstatus, uintptr_t mbadaddr)
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

  uintptr_t n = regs[17], arg0 = regs[10], arg1 = regs[11], retval, ipi_type;
  uintptr_t arg2, arg3, arg4, arg5;

  switch (n)
  {
    // SM calls from OS Kernel
    case SBI_SM_OS_SET_DMA_RANGE:
      retval = set_dma_range((uintptr_t)arg0, (uintptr_t)arg1);
      break;
    case SBI_SM_OS_DRAM_REGION_STATE:
      retval = dram_region_state((size_t)arg0);
      break;
    case SBI_SM_OS_DRAM_REGION_OWNER:
      retval = dram_region_owner((size_t)arg0);
      break;
    case SBI_SM_OS_ASSIGN_DRAM_REGION:
      retval = assign_dram_region((size_t)arg0, (enclave_id_t)arg1);
      break;
    case SBI_SM_OS_FREE_DRAM_REGION:
      retval = free_dram_region((size_t)arg0);
      break;
    case SBI_SM_OS_FLUSH_CACHED_DRAM_REGIONS:
      retval = flush_cached_dram_regions();
      break;
    case SBI_SM_OS_CREATE_METADATA_REGION:
      retval = create_metadata_region((size_t)arg0);
      break;
    case SBI_SM_OS_METADATA_REGION_PAGES:
      retval = metadata_region_pages();
      break;
    case SBI_SM_OS_METADATA_REGION_START:
      retval = metadata_region_start();
      break;
    case SBI_SM_OS_THREAD_METADATA_PAGES:
      retval = thread_metadata_pages();
      break;
    case SBI_SM_OS_ENCLAVE_METADATA_PAGES:
      retval = enclave_metadata_pages((size_t)arg0);
      break;
    case SBI_SM_OS_CREATE_ENCLAVE:
      arg2 = regs[12];
      arg3 = regs[13];
      arg4 = regs[14];
      retval = create_enclave((enclave_id_t)arg0, (uintptr_t)arg1,
          (uintptr_t)arg2, (size_t)arg3, (bool)arg4);
      break;
    case SBI_SM_OS_LOAD_PAGE_TABLE:
      arg2 = regs[12];
      arg3 = regs[13];
      arg4 = regs[14];
      retval = load_page_table((enclave_id_t)arg0, (uintptr_t)arg1,
          (uintptr_t)arg2, (size_t)arg3, (uintptr_t)arg4);
      break;
    case SBI_SM_OS_LOAD_PAGE:
      arg2 = regs[12];
      arg3 = regs[13];
      arg4 = regs[14];
      retval = load_page((enclave_id_t)arg0, (uintptr_t)arg1,
          (uintptr_t)arg2, (uintptr_t)arg3, (uintptr_t)arg4);
      break;
    case SBI_SM_OS_LOAD_THREAD:
      arg2 = regs[12];
      arg3 = regs[13];
      arg4 = regs[14];
      arg5 = regs[16];
      retval = load_thread((enclave_id_t)arg0, (thread_id_t)arg1,
          (uintptr_t)arg2, (uintptr_t)arg3, (uintptr_t)arg4,
          (uintptr_t)arg5);
      break;
    case SBI_SM_OS_ASSIGN_THREAD:
      retval = assign_thread((enclave_id_t)arg0, (thread_id_t)arg1);
      break;
    case SBI_SM_OS_INIT_ENCLAVE:
      retval = init_enclave((enclave_id_t)arg0);
      break;
    case SBI_SM_OS_ENTER_ENCLAVE:
      retval = enter_enclave((enclave_id_t)arg0, (thread_id_t)arg1);
      break;
    case SBI_SM_OS_DELETE_THREAD:
      retval = delete_thread((thread_id_t)arg0);
      break;
    case SBI_SM_OS_DELETE_ENCLAVE:
      retval = delete_enclave((enclave_id_t)arg0);
      break;
    case SBI_SM_OS_COPY_DEBUG_ENCLAVE_PAGE:
      arg2 = regs[12];
      arg3 = regs[13];
      retval = copy_debug_enclave_page((enclave_id_t)arg0,
          (uintptr_t)arg1, (uintptr_t)arg2, (bool)arg3);
      break;

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

void pmp_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  redirect_trap_to_s(mepc, read_csr(mstatus), read_csr(mbadaddr));
}

static void machine_page_fault(uintptr_t* regs, uintptr_t dummy, uintptr_t mepc)
{
  // MPRV=1 iff this trap occurred while emulating an instruction on behalf
  // of a lower privilege level. In that case, a2=epc and a3=mstatus.
  if (read_csr(mstatus) & MSTATUS_MPRV) {
    return redirect_trap_to_s(regs[12], regs[13], read_csr(mbadaddr));
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
  redirect_trap_to_s(mepc, read_csr(mstatus), read_csr(mbadaddr));
}

