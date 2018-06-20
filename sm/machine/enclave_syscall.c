
#include "csr.h"

#include "mcall.h"

#include <errno.h>
#include <string.h>

#define AES256 1
#define CTR 1
#include "aes/aes.h"

#define ED25519_NO_SEED 1
#include "ed25519/ed25519.h"

#include "sha3/sha3.h"

extern uint8_t PK_D[32];
extern uint8_t SM_H[64];
extern uint8_t PK_SM[32];
extern uint8_t SK_SM[64];
extern uint8_t SM_SIG[64];

uint64_t sm_fetch_field(void* out_field, uint64_t field_id) {
  uint64_t result = 0;
  switch (field_id) {
    case SM_FIELD_PK_D:
      memcpy(out_field, PK_D, 32);
      break;
    case SM_FIELD_H_SM:
      memcpy(out_field, SM_H, 64);
      break;
    case SM_FIELD_PK_SM:
      memcpy(out_field, PK_SM, 32);
      break;
    case SM_FIELD_SIGN_D:
      memcpy(out_field, SM_SIG, 64);
      break;
    default:
      // no effect
      result = -ENOSYS;
      break;
  }
  return result;
}

uint64_t sm_aes_cbc(void* buffer, uint8_t iv[16], uint32_t buffer_len) {
  if (buffer_len > 512) {
    return -ENOSYS;
  }

  uint8_t sm_iv[16];
  uint8_t sm_buffer[512];
  uint8_t sm_symkey[32];

  memcpy(sm_buffer, buffer, buffer_len);
  memcpy(sm_iv, iv, 12);
  sha3(SK_SM, 64, sm_symkey, 32);

  struct AES_ctx aes_context;
  AES_init_ctx_iv(&aes_context, sm_symkey, sm_iv);
  AES_CTR_xcrypt_buffer(&aes_context, sm_buffer, buffer_len);

  memcpy(buffer, sm_buffer, buffer_len);

  return 0;
}

uint64_t sm_sign_message(uint8_t out_signature[64], void* in_message, uint32_t message_len) {
  if (message_len > 512) {
    return -ENOSYS;
  }

  uint8_t sm_signature[64];
  uint8_t sm_message[512];

  memcpy(sm_message, in_message, message_len);
  ed25519_sign(sm_signature, sm_message, message_len, PK_SM, SK_SM);
  memcpy(out_signature, sm_signature, 64);

  return 0;
}

uint64_t sm_poet(uint8_t * out_hmac, uint8_t * out_signature, uint8_t * in_message, uint32_t in_message_len) {
  if (in_message_len > 512) {
    return -ENOSYS;
  }

  uint8_t sm_message[512];
  uint8_t sm_hash[32];
  uint8_t sm_signature[64];

  memcpy(sm_message, in_message, in_message_len);

  // HMAC
  sha3_ctx_t hash_ctx;
  sha3_init(&hash_ctx, 32);
  sha3_update(&hash_ctx, sm_message, in_message_len);
  sha3_update(&hash_ctx, SK_SM, 64);
  sha3_final(sm_hash, &hash_ctx);

  // Elapsed time
  for (int i=0; i<0xFFF; i++) {
    read_csr(0xCC0);
  }

  // Signature
  ed25519_sign(sm_signature, sm_hash, 32, PK_SM, SK_SM);

  memcpy(out_hmac, sm_hash, 32);
  memcpy(out_signature, sm_signature, 64);

  return 0;
}

