#ifndef BARE_PAGE_TABLES_H_INCLUDED
#define BARE_PAGE_TABLES_H_INCLUDED

#include "base_types.h"

// Number of bits in an address that don't undergo address translation.
const size_t page_shift() {
  return 12;
}

// NOTE: The constants below reflect RV39.

// Number of page table levels.
//
// We number levels from 0, which is assigned to the page table leaves.
//
// For example, in x86_64, the levels are 0 (PT), 1 (PD), 2 (PDPT), 3 (PML4),
// and page_table_levels is 4.
const size_t page_table_levels() {
  return 3;
}

// The number of address bits translated by a page table level.
//
// On most architectures, the number of bits is not level-dependent.
const inline size_t page_table_shift(size_t level) {
  return 9;
}

// Log2 of the size of a page table entry, at a given level, in bytes.
//
// On most architectures, each page table entry holds a pointer whose least
// significant bits are reused for access control flags. Therefore, this is
// generally log2(sizeof(uintptr_t)).
const inline size_t page_table_entry_shift(size_t level) {
  return 3;  // 8 bytes per page table entry
}

// Reads the valid (a.k.a. present) bit in a page table entry.
//
// Page entries with the valid bit unset have no other valid fields.
inline bool is_valid_page_table_entry(uintptr_t entry_addr, size_t level) {
  return *((bool*)entry_addr) & 1;
}

// Reads the destination pointer in a page table entry.
//
// The pointer can be the physical address of the next level page table, or the
// physical address for a virtual address.
inline uintptr_t page_table_entry_target(uintptr_t entry_addr, size_t level) {
  uintptr_t target_mask = ~((1 << page_shift()) - 1);
  return *((uintptr_t*)entry_addr) & target_mask;
}

// Writes a page table entry.
//
// `target` points to the next level page table, or has the physical address
// that comes from the translation. `acl` has platform-dependent access control
// flags, such as W (writable) and NX (not-executable).
//
// Before being combined with `target`, the `acl` value is masked against a
// value that only leaves in bits with known access control roles. For example,
// the valid / present bit will be masked off of the ACL.
inline void write_page_table_entry(uintptr_t entry_addr, size_t level,
    uintptr_t target, uintptr_t acl) {
  uintptr_t acl_mask = (1 << page_shift()) - 1;
  acl &= acl_mask;  // Mask off non-ACL bits.
  acl |= 1;         // Force valid to true.
  *((uintptr_t*)entry_addr) = target | acl;
}

// Computed values
// ---------------

// Page size in bytes.
const size_t page_size() {
  return 1 << page_shift();
}

// The size of a page table entry, at a given level, in bytes.
const inline size_t page_table_entry_size(size_t level) {
  return 1 << page_table_entry_shift(level);
}

// The number of page table entries at a given level.
const inline size_t page_table_entries(size_t level) {
  return 1 << page_table_shift(level);
}

// The size of a page table at a given level, in bytes.
const inline size_t page_table_size(size_t level) {
  return page_table_entries(level) * page_table_entry_size(level);
}

// The size of a page table at a given level, in pages.
const inline size_t page_table_pages(size_t level) {
  return page_table_size(level) >> page_shift();
}

// Used to implement page_table_translated_bits.
const inline size_t __page_table_translated_bits(size_t level, size_t sum) {
  return (level == page_table_levels()) ? sum :
      __page_table_translated_bits(level + 1, sum + page_table_shift(level));
}

// The total number of bits translated by the page table.
//
// This should be optimized to a constant by the compiler.
const inline size_t page_table_translated_bits() {
  return __page_table_translated_bits(0, page_shift());
}

#endif  // !defined(BARE_PAGE_TABLES_H_INCLUDED)
