#ifndef BARE_MEMORY_H_INCLUDED
#define BARE_MEMORY_H_INCLUDED

#include "base_types.h"

// Sets the DMARBASE (DMA range base) register in the DMA master.
//
// This can only be called by the security monitor.
static inline void set_dmar_base(uintptr_t value) {
  // TODO: asm intrinsics, but this implementation does not assume DMA, so do nothing
}

// Sets the DMARMASK (DMA range mask) register in the DMA master.
//
// This can only be called by the security monitor.
static inline void set_dmar_mask(uintptr_t value) {
  // TODO: asm intrinsics, but this implementation does not assume DMA, so do nothing
}

// Obtains the DRAM size from the memory subsystem.
//
// The implementation may be very slow, so the return value should be cached.
//
// The implementation may use privileged instructions.
static inline size_t read_dram_base() {
  return 0x80000000;
}

static inline size_t read_dram_size() {
  return 2147483648; // 2GB
}

// The number of levels of cache memory.
//
// The implementation may be very slow, so the return value should be cached.
//
// The implementation may use privileged instructions.
static inline size_t read_cache_levels() {
  return 2; // TODO: this is hard-wired here
}

// True if the given cache level is shared among cores.
//
// The implementation may be very slow, so the return value should be cached.
//
// The implementation may use privileged instructions.
static inline bool is_shared_cache(size_t cache_level) {
  return cache_level == 1; // only L2 is shared
}

// The size of a cache line at a given level.
//
// The implementation may be very slow, so the return value should be cached.
//
// The implementation may use privileged instructions.
static inline size_t read_cache_line_size(size_t cache_level) {
  return 64; // hard-wired to be 64B
}

// The number of cache sets at a given level.
//
// The implementation may be very slow, so the return value should be cached.
//
// The implementation may use privileged instructions.
static inline size_t read_cache_set_count(size_t cache_level) {
  return (cache_level == 0) ? 4096 : 32768; // 4096 lines per way, assume 8-way L2. Assume 8-way 256K L1
}

// The maximum value of the cache index shift for the platform.
static inline size_t read_min_cache_index_shift() {
  return 0; // TODO: this is irrelevant for this prototype
}

// The minimum value of the cache index shift for the platform.
static inline size_t read_max_cache_index_shift() {
  return 0; // TODO: this is irrelevant for this prototype
}

// Fills a buffer in physical memory with zeros.
//
// In order to allow for optimized assembly implementations, both the starting
// address and buffer size must be a multiple of the cache line size.
static inline void bzero(void* start, size_t bytes) {
  // Note: this is basically memset zero at 64b granularity
  memset(start, 0, bytes);
}

// Copies data between two non-overlaping buffers in physical memory.
//
// In order to allow for optimized assembly implementations, both addresses, as
// well as the buffer size must be a multiple of the cache line size.
static inline void bcopy(void* dest, void* source, size_t bytes) {
  // Note: this is basically memcopy at 64b granularity
  memcpy(dest, source, bytes);
}

#endif  // !defined(BARE_MEMORY_H_INCLUDED)
