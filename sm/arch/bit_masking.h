#ifndef BARE_BIT_MASKING_H_INCLUDED
#define BARE_BIT_MASKING_H_INCLUDED

#include "base_types.h"
#include "page_tables.h"

// Checks if an argument is a valid range mask.
//
// Valid range masks are written in binary as a sequence of 0s, followed by a
// sequence of 1s. Equivalently, the size of a base/mask range must be a power
// of two.
static inline const bool is_valid_range_mask(uintptr_t mask) {
  return (mask & (mask + 1)) == 0;
}
// Checks if an address is aligned with regard to a mask.
static inline const bool is_aligned_to_mask(uintptr_t address, uintptr_t mask) {
  return (address & mask) == 0;
}
// Checks the validity of a base/mask range.
//
// Valid range masks are written in binary as a sequence of 0s, followed by a
// sequence of 1s. Equivalently, the size of a base/mask range must be a power
// of two.
//
// A valid range base has 0s in the positions where the range mask has 1s.
// Equivalently, the range's base must be size-aligned.
static inline const bool is_valid_range(uintptr_t base, uintptr_t mask) {
  return is_aligned_to_mask(base, mask) && is_valid_range_mask(mask);
}
// Checks if an address is aligned to an address translation page.
static inline const bool is_page_aligned(uintptr_t address) {
  return is_aligned_to_mask(address, page_size() - 1);
}

// The smallest number of pages needed to store an amount of memory.
static inline const size_t pages_needed_for(size_t memory_size) {
  return (memory_size + page_size() - 1) >> page_shift();
}

// The number of bits needed to represent a quantity.
//
// This is 1 + the position of the most significant 1 bit in the number.
static inline size_t address_bits_for(size_t memory_size) {
  // NOTE: we could constexpr this by using a recursive implementation that
  //       relies on tail call optimization to not generate a large stack, but
  //       ensuring taill optimization is performed is a big hassle, and it's
  //       not worth it, given the current usage
  size_t bits = 0;
  for(memory_size -= 1; memory_size > 0; memory_size >>= 1)
    bits += 1;
  return bits;
}

// The smallest power of two that is greater or equal to a quantity.
static inline size_t ceil_power_of_two(size_t memory_size) {
  return 1 << address_bits_for(memory_size);
}

// True if the argument is a power of two.
static inline const bool is_power_of_two(size_t memory_size) {
  return memory_size > 0 && is_valid_range_mask(memory_size - 1);
}

// Sets or clears a bit in a bitmap.
//
// `value` is true for setting the bit, or false for clearing the bit.
static inline void set_bitmap_bit(size_t* bitmap, size_t bit, bool value) {
  const size_t bits_in_size_t = sizeof(size_t) * 8;

  // NOTE: relying on the compiler to optimize division to bitwise shift
  const size_t offset = bit / bits_in_size_t;
  const size_t mask = 1 << (bit % bits_in_size_t);

  if (value)
    *(bitmap + offset) |= mask;
  else
    *(bitmap + offset) &= ~mask;
}
// Returns the value of a bit in a bitmap.
static inline bool read_bitmap_bit(size_t* bitmap, size_t bit) {
  const size_t bits_in_size_t = sizeof(size_t) * 8;

  // NOTE: relying on the compiler to optimize division to bitwise shift
  const size_t offset = bit / bits_in_size_t;
  const size_t mask = 1 << (bit % bits_in_size_t);

  return (*(bitmap + offset) & mask) != 0;
}

// True if this is a big-endian architecture.
#define is_big_endian() false

#endif  // !defined(BARE_BIT_MASKING_H_INCLUDED)
