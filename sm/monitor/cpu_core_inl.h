#ifndef MONITOR_CPU_CORE_INL_H_INCLUDED
#define MONITOR_CPU_CORE_INL_H_INCLUDED

#include "cpu_core.h"

// The physical address of the core_info_t for the current core.
static inline core_info_t* current_core_info() {
  return &g_core[current_core()];
}

// The enclave running on the current core.
//
// Returns null_enclave_id if no enclave is running on the current core. This
// implies that the caller is the OS.
static inline enclave_id_t current_enclave() {
  core_info_t* core_info = current_core_info();
  return core_info->enclave_id;
}

#endif  // !defined(MONITOR_CPU_CORE_INL_H_INCLUDED)
