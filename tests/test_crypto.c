// Testes crypto: MD5, UUID offline, (OpenSSL:) RSA/AES/SHA1.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minecpp/core/crypto.h"

static int g_fail = 0;

#define ASSERT(cond)                                         \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      g_fail++;                                              \
    }                                                        \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static void hex16(const uint8_t d[16], char out[33]) {
  for (int i = 0; i < 16; i++) sprintf(out + 2 * i, "%02x", d[i]);
}

static void t_md5(void) {
  static const struct {
    const char *in, *want;
  } cases[] = {
      {"", "d41d8cd98f00b204e9800998ecf8427e"},
      {"abc", "900150983cd24fb0d6963f7d28e17f72"},
      {"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
      {"The quick brown fox jumps over the lazy dog",
       "9e107d9d372bb6826bd81d3542a419d6"},
      // >55 bytes força segundo bloco de padding (teste de borda real).
      {"12345678901234567890123456789012345678901234567890123456",
       "49f193adce178490e34d1b3a4ec0064c"},
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    uint8_t d[16];
    char h[33];
    minecpp_md5((const uint8_t *)cases[i].in, strlen(cases[i].in), d);
    hex16(d, h);
    ASSERT(strcmp(h, cases[i].want) == 0);
  }
}

static void t_offline_uuid(void) {
  uint8_t raw[16];
  char s[37];
  ASSERT_EQ(minecpp_offline_uuid("Notch", raw, s), 0);
  ASSERT(strcmp(s, "b50ad385-829d-3141-a216-7e7d7539ba7f") == 0);
  ASSERT_EQ(minecpp_offline_uuid("Minecpp", raw, s), 0);
  ASSERT(strcmp(s, "4ee4ebd9-458b-37d0-8c60-46e00dbe3399") == 0);
  // Golden do vanilla real (LoginSuccess capturado do server.jar 1.8).
  ASSERT_EQ(minecpp_offline_uuid("Goldens", raw, s), 0);
  ASSERT(strcmp(s, "4ef5afd9-c66f-3426-ac19-79e162599e71") == 0);
  // Bits de versão/variante.
  ASSERT_EQ(raw[6] >> 4, 3);
  ASSERT_EQ(raw[8] >> 6, 2);
  // Determinístico.
  char s2[37];
  ASSERT_EQ(minecpp_offline_uuid("Notch", raw, s2), 0);
  ASSERT(strcmp(s2, "b50ad385-829d-3141-a216-7e7d7539ba7f") == 0);
  // Inválidos.
  ASSERT_EQ(minecpp_offline_uuid("", raw, s), -1);
  ASSERT_EQ(minecpp_offline_uuid("12345678901234567", raw, s), -1);
  ASSERT_EQ(minecpp_offline_uuid(NULL, raw, s), -1);
}

#ifdef MINECPP_WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

static void t_rsa(void) {
  minecpp_rsa_t *k = minecpp_rsa_generate();
  ASSERT(k);
  if (!k) return;
  uint8_t *der = NULL;
  size_t dern = 0;
  ASSERT_EQ(minecpp_rsa_pubkey_der(k, &der, &dern), 0);
  ASSERT(der && dern > 100 && dern < 300 && der[0] == 0x30);  // SEQUENCE X.509
  // Round-trip: criptografa com a pública (EVP), descriptografa com a nossa.
  static const uint8_t secret[16] = {0, 1, 2, 3, 4,  5,  6,  7,
                                     8, 9, 10, 11, 12, 13, 14, 15};
  const uint8_t *w = der;
  EVP_PKEY *pub = d2i_PUBKEY(NULL, &w, (long)dern);
  ASSERT(pub);
  uint8_t enc[128] = {0};
  size_t enclen = sizeof enc;
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pub, NULL);
  ASSERT(ctx && EVP_PKEY_encrypt_init(ctx) == 1 &&
         EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) == 1 &&
         EVP_PKEY_encrypt(ctx, enc, &enclen, secret, sizeof secret) == 1 &&
         enclen == 128);
  EVP_PKEY_CTX_free(ctx);
  EVP_PKEY_free(pub);
  uint8_t dec[128] = {0};
  size_t declen = 0;
  ASSERT_EQ(minecpp_rsa_decrypt(k, enc, dec, &declen), 0);
  ASSERT_EQ(declen, sizeof secret);
  ASSERT(memcmp(dec, secret, sizeof secret) == 0);
  // Bloco inválido: OpenSSL 3 usa implicit rejection (pode retornar 0 com
  // bytes pseudo-aleatórios em vez de erro) — o que importa é NÃO recuperar
  // o segredo original.
  memset(enc, 0xAA, sizeof enc);
  declen = 0;
  memset(dec, 0, sizeof dec);
  minecpp_rsa_decrypt(k, enc, dec, &declen);
  ASSERT(!(declen == sizeof secret && memcmp(dec, secret, sizeof secret) == 0));
  free(der);
  minecpp_rsa_free(k);
}

static void t_aes(void) {
  const uint8_t key[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  uint8_t plain[100], c1[100], c2[100], back[100];
  for (int i = 0; i < 100; i++) plain[i] = (uint8_t)(i * 31 + 7);
  minecpp_aes_t *e = minecpp_aes_encrypt_init(key);
  minecpp_aes_t *d = minecpp_aes_decrypt_init(key);
  ASSERT(e && d);
  // Streaming fracionado == bloco único (CFB8 é byte a byte).
  minecpp_aes_update(e, plain, c1, 100);
  minecpp_aes_free(e);
  e = minecpp_aes_encrypt_init(key);
  minecpp_aes_update(e, plain, c2, 37);
  minecpp_aes_update(e, plain + 37, c2 + 37, 63);
  ASSERT(memcmp(c1, c2, 100) == 0);
  minecpp_aes_update(d, c1, back, 100);
  ASSERT(memcmp(back, plain, 100) == 0);
  // Cifrado difere do plano.
  ASSERT(memcmp(c1, plain, 100) != 0);
  minecpp_aes_free(e);
  minecpp_aes_free(d);
}

static void t_sha1(void) {
  uint8_t out[20];
  char h[41];
  ASSERT_EQ(minecpp_sha1((const uint8_t *)"abc", 3, out), 0);
  for (int i = 0; i < 20; i++) sprintf(h + 2 * i, "%02x", out[i]);
  ASSERT(strcmp(h, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
}
#endif

int main(void) {
  t_md5();
  t_offline_uuid();
#ifdef MINECPP_WITH_OPENSSL
  t_rsa();
  t_aes();
  t_sha1();
#else
  printf("(sem OpenSSL: RSA/AES/SHA1 pulados)\n");
#endif
  if (g_fail == 0) printf("crypto: todas as verificações passaram\n");
  return g_fail ? 1 : 0;
}
