#ifndef MONITOR_MEASURE_INL_H_INCLUDED
#define MONITOR_MEASURE_INL_H_INCLUDED

#include <arch/memory.h>
#include <arch/page_tables.h>
#include <crypto/hash.h>
#include "enclave.h"
#include <public/api.h>

// The layout of a hash block used to measure an enclave operation.
//
// The structure may not overlap the entire hash block, so init_enclave_hash()
// zeroes the entire hash block. Every other function that uses the structure
// is responsible for zeroing out the arguments that it sets after it's done
// extending the enclave hash. The opcode field does not need to be zeroed out,
// because every operation needs to set it.
typedef struct {
  size_t opcode;
  uintptr_t ptr1, ptr2, ptr3, ptr4;
  size_t size1, size2;
} measurement_block_t;

//static_assert(sizeof(measurement_block_t) <= hash_block_size,
//    "measurement_block_t does not fit in a hash block");

// Opcodes for enclave operations.
const size_t enclave_init_opcode =      0xAAAAAAAA;
const size_t load_page_table_opcode =   0xBBBBBBBB;
const size_t load_page_opcode =         0xCCCCCCCC;
const size_t load_thread_opcode =       0xDDDDDDDD;
const size_t finalize_enclave_opcode =  0xEEEEEEEE;

// Computes the address of an enclave's buffer for measurement hashing.
//
// The buffer is used to put together
static inline measurement_block_t* enclave_measurement_block(
    enclave_info_t* enclave_info) {
  uint32_t* block_ptr = enclave_info->hash_block;
  return block_ptr;
}

// Initializes an enclave's measurement hash.
//
// The caller must hold the lock of the enclave's main DRAM region.
static inline void init_enclave_hash(enclave_info_t* enclave_info,
    uintptr_t ev_base, uintptr_t ev_mask, size_t mailbox_count, bool debug) {
  // NOTE: 32-bit operations may be slow on 64-bit architectures, so we convert
  //       the pointer to an architecture-native type before instantiating the
  //       bzero template
  size_t* fast_hash_block = (size_t*)(enclave_info->hash_block);
  bzero(fast_hash_block, hash_block_size);
  init_hash(&(enclave_info->hash));

  measurement_block_t* block =
      enclave_measurement_block(enclave_info);
  block->opcode =   enclave_init_opcode;
  block->ptr1 =     ev_base;
  block->ptr2 =     ev_mask;
  block->size1 =    mailbox_count;
  block->size2 =    debug;

  extend_hash(&(enclave_info->hash),
      enclave_info->hash_block);
  block->ptr1 =     0;
  block->ptr2 =     0;
  block->size1 =    0;
  block->size2 =    0;
}

// Adds a page table creation operation to an enclave's measurement hash.
//
// The caller must hold the lock of the encalve's main DRAM region.
static inline void extend_enclave_hash_with_page_table(
    enclave_info_t* enclave_info, uintptr_t virtual_addr,
    size_t level, uintptr_t acl) {
  measurement_block_t* block =
      enclave_measurement_block(enclave_info);
  block->opcode =   load_page_table_opcode;
  block->ptr1 =     virtual_addr;
  block->ptr2 =     acl;
  block->size1 =    level;

  extend_hash(&(enclave_info->hash),
      enclave_info->hash_block);
  block->ptr1 =     0;
  block->ptr2 =     0;
  block->size1 =    0;
}

// Adds a page creation operation to an enclave's measurement hash.
//
// The caller must hold the lock of the encalve's main DRAM region.
//
// `phys_addr` is not included in the measurement. It's used to read in the
// page and hash its contents.
static inline void extend_enclave_hash_with_page(
    enclave_info_t* enclave_info, uintptr_t virtual_addr,
    uintptr_t acl, uintptr_t phys_addr) {
  measurement_block_t* block =
      enclave_measurement_block(enclave_info);
  block->opcode = load_page_opcode;
  block->ptr1 =   virtual_addr;
  block->ptr2 =   acl;

  extend_hash(&(enclave_info->hash),
      enclave_info->hash_block);
  block->ptr1 = 0;
  block->ptr2 = 0;

  uint32_t* page_end = phys_addr + page_size();
  for(uint32_t* page_ptr = phys_addr; page_ptr != page_end;
      page_ptr += hash_block_size / sizeof(uint32_t)) {
    extend_hash(&(enclave_info->hash), page_ptr);
  }
}

// Adds a thread creation operation to an enclave's measurement hash.
//
// The caller must hold the lock of the encalve's main DRAM region.
static inline void extend_enclave_hash_with_thread(
    enclave_info_t* enclave_info, uintptr_t entry_pc,
    uintptr_t entry_stack, uintptr_t fault_pc, uintptr_t fault_stack) {
  measurement_block_t* block =
      enclave_measurement_block(enclave_info);
  block->opcode = load_thread_opcode;
  block->ptr1 =   entry_pc;
  block->ptr2 =   entry_stack;
  block->ptr3 =   fault_pc;
  block->ptr4 =   fault_stack;

  extend_hash(&(enclave_info->hash),
      enclave_info->hash_block);
  block->ptr1 = 0;
  block->ptr2 = 0;
  block->ptr3 = 0;
  block->ptr4 = 0;
}

// Finalizes the enclave's measurement hash.
static inline void finalize_enclave_hash(enclave_info_t* enclave_info) {
  measurement_block_t* block =
      enclave_measurement_block(enclave_info);
  block->opcode = finalize_enclave_opcode;

  extend_hash(&(enclave_info->hash),
      enclave_info->hash_block);
  finalize_hash(&(enclave_info->hash));
}

#endif  // !defined(MONITOR_MEASURE_INL_H_INCLUDED)
