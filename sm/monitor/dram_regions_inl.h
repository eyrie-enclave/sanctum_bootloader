#ifndef MONITOR_DRAM_REGIONS_INL_H_INCLUDED
#define MONITOR_DRAM_REGIONS_INL_H_INCLUDED

#include <arch/bit_masking.h>
#include <arch/cpu_context.h>
#include <arch/memory.h>
#include "cpu_core.h"
#include "dram_regions.h"

// Verifies that a physical address belongs in DRAM.
//
// Physical addresses can also point to space belonging to memory-mapped
// devices, or to invalid physical addresses.
static inline bool is_dram_address(uintptr_t address) {
  return (address < g_dram_size) && (address > g_dram_base);
}

// True for valid DRAM region indices.
//
// This can be called without holding locks, as it relies on constant state.
static inline bool is_valid_dram_region(size_t dram_region) {
  return dram_region < g_dram_region_count;
}
// True for DRAM regions that can be freed and re-assigned.
//
// The first DRAM region contains monitor code, so it can only be assigned to
// the OS.
static inline bool is_dynamic_dram_region(size_t dram_region) {
  return dram_region != 0 && is_valid_dram_region(dram_region);
}

// Computes the physical start address of a DRAM region.
//
// Invalid DRAM region indices will yield invalid pointers.
static inline uintptr_t dram_region_start(size_t dram_region) {
  return (uintptr_t)((dram_region) << g_dram_region_shift);
}

// Computes the DRAM region index for a pointer.
//
// Pointers outside DRAM will yield valid but meaningless region indices.
static inline size_t dram_region_for(uintptr_t address) {
  return (address & g_dram_region_mask) >> g_dram_region_shift;
}

// Computes the DRAM region index for a pointer.
//
// Region index 0 will be returned for pointers outside DRAM.
static inline size_t clamped_dram_region_for(uintptr_t address) {
  return is_dram_address(address) ? dram_region_for(address) : 0;
}

// Computes the DRAM region stripe page index for a pointer.
//
// Pointers outside DRAM will yield valid but meaningless stripe indices.
static inline size_t dram_stripe_page_for(uintptr_t address) {
  return (address & g_dram_stripe_page_mask) >> page_shift();
}

// Computes the DRAM region stripe index for a pointer.
//
// Pointers outside DRAM will yield invalid page indices.
static inline size_t dram_stripe_for(uintptr_t address) {
  return address >> g_dram_stripe_shift;
}

// Computes the DRAM region page index for a pointer.
//
// The DRAM region page index is unique for a page within a DRAM region,
// whereas the stripe page index is only unique within a DRAM region stripe.
//
// Pointers outside DRAM will yield invalid page indices.
static inline size_t dram_region_page_for(uintptr_t address) {
  return dram_stripe_page_for(address) | (dram_stripe_for(address) <<
      (g_dram_region_shift - g_dram_stripe_shift));
}

// Acquires the lock for a DRAM region.
//
// Invalid DRAM region indices will cause memory thrashing.
//
// Returns false if the lock was acquired, and true if it was already held by
// someone else.
static inline bool test_and_set_dram_region_lock(size_t dram_region) {
  dram_region_info_t* region = &g_dram_region[dram_region];
  return atomic_flag_test_and_set(region->lock);
}

// Releases the lock for a DRAM region.
//
// Invalid DRAM region indices will cause memory thrashing.
//
// Clear a lock that was not explicitly acquired is a security vulnerability,
// because another piece of code might have acquired the lock.
static inline void clear_dram_region_lock(size_t dram_region) {
  dram_region_info_t* region = &g_dram_region[dram_region];
  atomic_flag_clear(region->lock);
}

// Reads the owner from a DRAM region.
//
// Invalid DRAM region indices will cause memory reads outside the DRAM space.
static inline enclave_id_t read_dram_region_owner(size_t dram_region) {
  const dram_region_info_t* region = &g_dram_region[dram_region];
  return region->owner;
}

// Wipes the data in a DRAM region.
//
// Invalid DRAM region indices will cause memory trashing.
static inline void bzero_dram_region(size_t dram_region) {
  // The address diff between two stripes belonging to the same DRAM region.
  const uintptr_t stripe_step = g_dram_region_count << g_dram_region_shift;

  const uintptr_t region_start = dram_region << g_dram_region_shift;
  for (uintptr_t stripe = 0; stripe < g_dram_size; stripe += stripe_step) {
    const uintptr_t stripe_start = stripe | region_start;
    bzero((size_t*)stripe_start, g_dram_stripe_size);
  }
}

// Flushes the core's TLBs and updates the relevant flush generation counter.
//
// This code is guaranteed to be lock-free, as it is used in enclave exits.
static inline void dram_region_tlb_flush() {
  flush_tlbs(); // sanctum::bare::flush_tlbs();

  // NOTE: it is safer to increment the atomic counter AFTER the TLBs are
  //       flushed; the moment the counter is incremented, some DRAM region may
  //       be freed and reallocated; this sequence is

  const core_info_t* core_info = &g_core[current_core()];
  const size_t block_clock = atomic_load(
      &(g_dram_regions->block_clock));
  atomic_store(core_info->flushed_at, block_clock);
}

#endif  // !defined(MONITOR_DRAM_REGIONS_INL_H_INCLUDED)
