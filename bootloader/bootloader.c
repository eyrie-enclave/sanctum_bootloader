#include <stdint.h>

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

#define ED25519_NO_SEED 1
#include "ed25519/ed25519.h"
/* Adopted from https://github.com/orlp/ed25519
  Modified to use SHA3
  provides:
  - void ed25519_create_keypair(t_pubkey *public_key, t_privkey *private_key, t_seed *seed);
  - void ed25519_sign(t_signature *signature,
                      const unsigned uint8_t *message,
                      size_t message_len,
                      t_pubkey *public_key,
                      t_privkey *private_key);
*/

#include  "csr.h"
/*
  provides macros:
  - read_csr(reg)
  - write_csr(reg, val)
  - swap_csr(reg, val)
  provides constants:
  - CSR_TRNG, CSR_MPUFSELECT, CSR_MPUFDISABLE, CSR_MPUFREADOUT
*/

#include "randomart.h"
/*
  privides:
  - randomart(uint8_t *input, size_t len, char *out_str)
*/

extern uint8_t PK_D[32];
// SK_D is kept on the stack, and later erased
extern uint8_t SM_H[64];
extern uint8_t PK_SM[32];
extern uint8_t SK_SM[64];
extern uint8_t SM_SIG[64];

extern const void* sm_ptr;
extern const size_t sm_size;

extern volatile uint64_t fromhost;
extern volatile uint64_t tohost;

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

void print_str(char* s) {
  while (*s != 0) {
    print_char(*s++);
  }
}

void print_nibble(uint8_t n)  {
  if (n < 0xA) {
    n += '0';
  } else {
    n += ('A' - 0xA);
  }
  print_char(n);
}

void print_hex_byte(uint8_t b) {
  print_char('\\');
  print_char('x');
  print_nibble((b >> 4) & 0xF); // high-order nibble
  print_nibble(b & 0xF);        // low-order nibble
}

void __attribute__ ((section (".text.bootloader.entry"))) rot_bootloader() {
  uint8_t scratchpad[64];
  sha3_ctx_t hash_ctx;
  uint8_t SK_D[64];

  //print_str("Hello!\n");

  // Random seed from TRNG
  for (int i=0; i<4; i++) {
    ((uint64_t*)scratchpad)[i] = read_csr(0xCC0); // TRNG
  }

  // Derive device keys PD_D, SK_D
  ed25519_create_keypair(PK_D, SK_D, scratchpad);

  // Commit to PK_D by printing it to the host
  print_str("Committing to PK_D: (ed25519) ");
  for (int i=0; i<32; i++) {
    print_hex_byte(PK_D[i]);
  }

  char randomart_str[256];
  randomart(PK_D, 32, randomart_str);
  print_str("\n\n+--[ED25519 256]--+\n");
  print_str(randomart_str);
  print_str("\nTrusted manufacturer (host) should endorse this key in order to root the device's attestations.\n\n");

  // Measure the SM
  sha3(&sm_ptr, sm_size, SM_H, 64);

  // Derive SM keys from H(SK_D, H(SM))
  // scratchpad <-- H(SK_D H(SM))

  sha3_init(&hash_ctx, 32);
  sha3_update(&hash_ctx, SK_D, 64);
  sha3_update(&hash_ctx, SM_H, 64);
  sha3_final(scratchpad, &hash_ctx);
  // derive keys from seed in scratchpad
  ed25519_create_keypair(PK_SM, SK_SM, scratchpad);

  // Endorse the SM with PK_D
  // scratchpad <-- H(H(SM), PK_SM)
  sha3_init(&hash_ctx, 64);
  sha3_update(&hash_ctx, SM_H, 64);
  sha3_update(&hash_ctx, PK_SM, 32);
  sha3_final(scratchpad, &hash_ctx);
  // Sign scratchpad with SK_D
  ed25519_sign(SM_SIG, scratchpad, 64, PK_D, SK_D);

  return; // NOTE: stack and hart state will be cleaned before boot
}
