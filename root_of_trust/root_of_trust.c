#include <stdint.h>
#include <stdbool.h>

#include "sha3/sha3.h"
/*
  adopted from https://github.com/mjosaarinen/tiny_sha3 commit dcbb3192047c2a721f5f851db591871d428036a9
  provides:
  - void * sha3(const void *message, size_t message_bytes, void *output, int output_bytes)
  - int sha3_init(sha3_ctx_t *c, int output_bytes);
  - int sha3_update(sha3_ctx_t *c, const void *message, size_t message_bytes);
  - int sha3_final(void *output, sha3_ctx_t *c);
  types: sha3_ctx_t
*/

extern const void * bootloader_ptr;
extern const size_t bootloader_size;
extern const uint64_t bootloader_expected_hash[8];

//extern volatile uint64_t fromhost;
//extern volatile uint64_t tohost;

/*
# define TOHOST_CMD(dev, cmd, payload) \
  (((uint64_t)(dev) << 56) | ((uint64_t)(cmd) << 48) | (uint64_t)(payload))

void print_char(char c) {
  // No synchronization needed, as the bootloader runs solely on core 0

  while (tohost) {
    // spin
    fromhost = 0;
  }

  tohost = TOHOST_CMD(1, 1, c); // send char
}
*/

bool __attribute__ ((section (".text.rot"))) rot_hash_and_verify() {
  // Reserve stack space for secret
  uint64_t bootloader_hash[8]; // 512-bit (64 byte) hash

  //print_char('@');

  // Compute hash of the trusted bootloader DRAM
  sha3(&bootloader_ptr, bootloader_size, bootloader_hash, 64);

  // Compare the root of trust hash againt an expected value
  // (there are no secrets to clean up);
  for (int i=0; i<8; i++){
    if (bootloader_hash[i] != bootloader_expected_hash[i]) {
      return false;
    }
  }
  return true;
}

