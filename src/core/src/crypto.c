#include "minecpp/core/crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- MD5 (RFC 1321, para UUID offline apenas)

static const uint32_t kMd5K[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu,
    0x4787c62au, 0xa8304613u, 0xfd469501u, 0x698098d8u, 0x8b44f7afu,
    0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu,
    0x49b40821u, 0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
    0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u, 0x21e1cde6u,
    0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u,
    0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u,
    0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u,
    0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u, 0xf4292244u, 0x432aff97u,
    0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du,
    0x85845dd1u, 0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
};

static const uint8_t kMd5S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

static uint32_t rol32(uint32_t v, unsigned n) { return v << n | v >> (32 - n); }

void minecpp_md5(const uint8_t *data, size_t len, uint8_t out[16]) {
  uint32_t a = 0x67452301u, b = 0xefcdab89u, c = 0x98badcfeu, d = 0x10325476u;
  // Mensagem + padding 0x80 + zeros + len-bits LE (streaming em blocos).
  uint64_t bitlen = (uint64_t)len * 8;
  size_t pos = 0;
  uint8_t block[64];
  size_t used = 0;

  // Macro local: despeja bloco cheio no estado.
#define MD5_ROUND()                                                      \
  do {                                                                   \
    uint32_t M[16], A = a, B = b, C = c, D = d;                          \
    for (int i = 0; i < 16; i++)                                         \
      M[i] = (uint32_t)block[i * 4] | (uint32_t)block[i * 4 + 1] << 8 |  \
             (uint32_t)block[i * 4 + 2] << 16 |                          \
             (uint32_t)block[i * 4 + 3] << 24;                           \
    for (int i = 0; i < 64; i++) {                                       \
      uint32_t F, g;                                                     \
      if (i < 16) {                                                      \
        F = D ^ (B & (C ^ D));                                           \
        g = (unsigned)i;                                                 \
      } else if (i < 32) {                                               \
        F = C ^ (D & (B ^ C));                                           \
        g = (unsigned)((5 * i + 1) & 15);                                \
      } else if (i < 48) {                                               \
        F = B ^ C ^ D;                                                   \
        g = (unsigned)((3 * i + 5) & 15);                                \
      } else {                                                           \
        F = C ^ (B | ~D);                                                \
        g = (unsigned)((7 * i) & 15);                                    \
      }                                                                  \
      F = F + A + kMd5K[i] + M[g];                                       \
      A = D;                                                             \
      D = C;                                                             \
      C = B;                                                             \
      B = B + rol32(F, kMd5S[i]);                                        \
    }                                                                    \
    a += A;                                                              \
    b += B;                                                              \
    c += C;                                                              \
    d += D;                                                              \
  } while (0)

  while (pos < len) {
    const size_t take = len - pos > 64 - used ? 64 - used : len - pos;
    memcpy(block + used, data + pos, take);
    used += take;
    pos += take;
    if (used == 64) {
      MD5_ROUND();
      used = 0;
    }
  }
  block[used++] = 0x80;
  if (used > 56) {
    memset(block + used, 0, 64 - used);
    MD5_ROUND();
    used = 0;
  }
  memset(block + used, 0, 56 - used);
  for (int i = 0; i < 8; i++) block[56 + i] = (uint8_t)(bitlen >> (8 * i));
  MD5_ROUND();

  const uint32_t st[4] = {a, b, c, d};
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) out[i * 4 + j] = (uint8_t)(st[i] >> (8 * j));
#undef MD5_ROUND
}

int minecpp_offline_uuid(const char *name, uint8_t raw16[16],
                         char str37[37]) {
  char src[64];
  int n;
  if (!name || !raw16) return -1;
  n = snprintf(src, sizeof src, "OfflinePlayer:%s", name);
  if (n <= 14 || n >= (int)sizeof src) return -1;  // vazio ou >16 chars
  if ((size_t)n - 14 > 16) return -1;
  minecpp_md5((const uint8_t *)src, (size_t)n, raw16);
  raw16[6] = (uint8_t)((raw16[6] & 0x0F) | 0x30);  // version 3
  raw16[8] = (uint8_t)((raw16[8] & 0x3F) | 0x80);  // variant RFC 4122
  if (str37) {
    snprintf(str37, 37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%"
             "02x%02x",
             raw16[0], raw16[1], raw16[2], raw16[3], raw16[4], raw16[5],
             raw16[6], raw16[7], raw16[8], raw16[9], raw16[10], raw16[11],
             raw16[12], raw16[13], raw16[14], raw16[15]);
  }
  return 0;
}

// ---------------------------------------------------------------- OpenSSL (RSA/AES/SHA1)

#ifdef MINECPP_WITH_OPENSSL

// EVP apenas (a API RSA_* direta está depreciada no OpenSSL 3 + -Werror).
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>  // i2d_PUBKEY

struct minecpp_rsa {
  EVP_PKEY *k;
};

struct minecpp_aes {
  EVP_CIPHER_CTX *ctx;
};

minecpp_rsa_t *minecpp_rsa_generate(void) {
  minecpp_rsa_t *r = (minecpp_rsa_t *)calloc(1, sizeof *r);
  if (!r) return NULL;
  r->k = EVP_RSA_gen(1024);  // vanilla usa 1024-bit
  if (!r->k) {
    free(r);
    return NULL;
  }
  return r;
}

void minecpp_rsa_free(minecpp_rsa_t *k) {
  if (!k) return;
  EVP_PKEY_free(k->k);
  free(k);
}

// X.509 SubjectPublicKeyInfo (igual ao Java key.getEncoded() do vanilla).
int minecpp_rsa_pubkey_der(minecpp_rsa_t *k, uint8_t **out, size_t *out_n) {
  int n;
  uint8_t *p;
  if (!k || !out || !out_n) return -1;
  n = i2d_PUBKEY(k->k, NULL);
  if (n <= 0) return -1;
  p = (uint8_t *)malloc((size_t)n);
  if (!p) return -1;
  {
    uint8_t *w = p;
    if (i2d_PUBKEY(k->k, &w) != n) {
      free(p);
      return -1;
    }
  }
  *out = p;
  *out_n = (size_t)n;
  return 0;
}

int minecpp_rsa_decrypt(minecpp_rsa_t *k, const uint8_t in128[128],
                        uint8_t *out, size_t *out_n) {
  EVP_PKEY_CTX *ctx = NULL;
  size_t n = 128;
  int rc = -1;
  if (!k || !in128 || !out || !out_n) return -1;
  ctx = EVP_PKEY_CTX_new(k->k, NULL);
  if (!ctx) return -1;
  if (EVP_PKEY_decrypt_init(ctx) == 1 &&
      EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) == 1 &&
      EVP_PKEY_decrypt(ctx, out, &n, in128, 128) == 1) {
    *out_n = n;
    rc = 0;
  }
  EVP_PKEY_CTX_free(ctx);
  return rc;
}

static minecpp_aes_t *aes_init(const uint8_t key[16], int enc) {
  minecpp_aes_t *a = (minecpp_aes_t *)calloc(1, sizeof *a);
  if (!a || !key) {
    free(a);
    return NULL;
  }
  a->ctx = EVP_CIPHER_CTX_new();
  // CFB8 com IV = chave (igual ao vanilla: secret como key e IV).
  if (!a->ctx ||
      EVP_CipherInit_ex(a->ctx, EVP_aes_128_cfb8(), NULL, key, key, enc) !=
          1 ||
      EVP_CIPHER_CTX_set_padding(a->ctx, 0) != 1) {
    EVP_CIPHER_CTX_free(a->ctx);
    free(a);
    return NULL;
  }
  return a;
}

minecpp_aes_t *minecpp_aes_encrypt_init(const uint8_t key[16]) {
  return aes_init(key, 1);
}

minecpp_aes_t *minecpp_aes_decrypt_init(const uint8_t key[16]) {
  return aes_init(key, 0);
}

void minecpp_aes_update(minecpp_aes_t *a, const uint8_t *in, uint8_t *out,
                        size_t n) {
  int olen = 0;
  if (!a || (!in && n) || (!out && n)) return;
  // CFB8 é streaming 1:1 sem padding/buffer interno relevante.
  while (n) {
    int step = n > 4096 ? 4096 : (int)n;
    if (EVP_CipherUpdate(a->ctx, out, &olen, in, step) != 1) return;
    in += step;
    out += olen;
    n -= (size_t)step;
  }
}

void minecpp_aes_free(minecpp_aes_t *a) {
  if (!a) return;
  EVP_CIPHER_CTX_free(a->ctx);
  free(a);
}

int minecpp_sha1(const uint8_t *data, size_t len, uint8_t out[20]) {
  if (!data || !out) return -1;
  SHA1(data, len, out);
  return 0;
}

#else  // sem OpenSSL: stubs honestos

struct minecpp_rsa {
  int unused;
};
struct minecpp_aes {
  int unused;
};

minecpp_rsa_t *minecpp_rsa_generate(void) { return NULL; }
void minecpp_rsa_free(minecpp_rsa_t *k) { (void)k; }
int minecpp_rsa_pubkey_der(minecpp_rsa_t *k, uint8_t **out, size_t *out_n) {
  (void)k;
  (void)out;
  (void)out_n;
  return -1;
}
int minecpp_rsa_decrypt(minecpp_rsa_t *k, const uint8_t in128[128],
                        uint8_t *out, size_t *out_n) {
  (void)k;
  (void)in128;
  (void)out;
  (void)out_n;
  return -1;
}
minecpp_aes_t *minecpp_aes_encrypt_init(const uint8_t key[16]) {
  (void)key;
  return NULL;
}
minecpp_aes_t *minecpp_aes_decrypt_init(const uint8_t key[16]) {
  (void)key;
  return NULL;
}
void minecpp_aes_update(minecpp_aes_t *a, const uint8_t *in, uint8_t *out,
                        size_t n) {
  (void)a;
  (void)in;
  (void)out;
  (void)n;
}
void minecpp_aes_free(minecpp_aes_t *a) { (void)a; }
int minecpp_sha1(const uint8_t *data, size_t len, uint8_t out[20]) {
  (void)data;
  (void)len;
  (void)out;
  return -1;
}

#endif
