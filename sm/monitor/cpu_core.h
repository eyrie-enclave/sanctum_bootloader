#ifndef MONITOR_CPU_CORE_H_INCLUDED
#define MONITOR_CPU_CORE_H_INCLUDED

#include <arch/base_types.h>
#include <arch/cpu_context.h>
#include <arch/atomics.h>
#include <public/api.h>
#include "enclave.h"

// Per-core accounting information.
//
// Most information is only modified on the core that it corresponds to, so we
// don't need atomics or locking. The notable exception is the flushed_at
// timestamp.
typedef struct {
  enclave_id_t enclave_id;  // 0 if the core isn't executing enclave code
  thread_id_t thread_id;

  // The DRAM backing the thread_info_t is guaranteed to be pinned
  // while the thread is executing on a core.
  thread_info_t* thread;

  // The value of block_clock when this core's TLB was last flushed.
  // This is read on other cores,
  size_t flushed_at;
} core_info_t;

// Core costants.
//
// These values are computed during the boot process. Once computed, the values
// never change.

extern core_info_t* g_core;

extern size_t g_core_count;

#endif  // !defined(MONITOR_CPU_CORE_H_INCLUDED)
