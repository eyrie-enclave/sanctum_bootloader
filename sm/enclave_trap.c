#include <machine/mtrap.h>
#include <machine/mcall.h>
#include <machine/atomic.h>
#include <machine/bits.h>
#include <machine/vm.h>
#include <machine/fdt.h>
#include <machine/unprivileged_memory.h>
#include <devices/uart.h>
#include <devices/htif.h>
#include <platform/platform_interface.h>
#include <public/api.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

// Route Enclave mode traps
// ========================
void ecall_from_enclave_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  write_csr(mepc, mepc + 4);

  uintptr_t n = regs[17], arg0 = regs[10], arg1 = regs[11], retval, ipi_type;
  uintptr_t arg2;

  switch (n)
  {
    // SM calls from Enclave Kernel
    case UBI_SM_ENCLAVE_BLOCK_DRAM_REGION:
      retval = block_dram_region((size_t)arg0);
      break;
    case UBI_SM_ENCLAVE_CHECK_OWNERSHIP:
      retval = dram_region_check_ownership((size_t)arg0);
      break;
    case UBI_SM_ENCLAVE_ACCEPT_THREAD:
      retval = accept_thread((thread_id_t)arg0, (uintptr_t)arg1);
      break;
    case UBI_SM_ENCLAVE_EXIT_ENCLAVE:
      retval = exit_enclave();
      break;
    case UBI_SM_ENCLAVE_GET_ATTESTATION_KEY:
      retval = get_attestation_key((uintptr_t)arg0);
      break;
    case UBI_SM_ENCLAVE_ACCEPT_MESSAGE:
      retval = accept_message((mailbox_id_t)arg0, (uintptr_t)arg1);
      break;
    case UBI_SM_ENCLAVE_READ_MESSAGE:
      retval = read_message((mailbox_id_t)arg0, (uintptr_t)arg1);
      break;
    case UBI_SM_ENCLAVE_SEND_MESSAGE:
      arg2 = regs[12],
      retval = send_message((enclave_id_t)arg0, (mailbox_id_t)arg1,
          (uintptr_t)arg2);
      break;
    default:
      retval = -ENOSYS;
      break;
  }
  regs[10] = retval;
}
