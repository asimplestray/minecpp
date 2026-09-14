#pragma once

// Criptografia do login vanilla + UUID offline.
//
// - MD5 e offline-UUID: sempre disponíveis (MD5 implementado aqui da RFC 1321,
//   só para UUID — nunca para segurança).
// - RSA/AES/SHA1: só com OpenSSL (MINECPP_WITH_OPENSSL). Sem ele, as funções
//   retornam NULL/-1 e o servidor opera em modo offline (fluxo sem cripto,
//   igual ao vanilla com online-mode=false).

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void minecpp_md5(const uint8_t *data, size_t len, uint8_t out[16]);

// UUID v3 de "OfflinePlayer:"+name (igual ao vanilla offline).
// raw16 sempre preenchido; str37 opcional ("8-4-4-4-12" minúsculo + NUL).
// Retorna 0 ok, -1 nome inválido (>16 chars ou vazio).
int minecpp_offline_uuid(const char *name, uint8_t raw16[16],
                         char str37[37]);

typedef struct minecpp_rsa minecpp_rsa_t;

minecpp_rsa_t *minecpp_rsa_generate(void);  // 1024-bit; NULL sem OpenSSL
void minecpp_rsa_free(minecpp_rsa_t *k);
// DER SubjectPublicKeyInfo (malloc; free nele).
int minecpp_rsa_pubkey_der(minecpp_rsa_t *k, uint8_t **out, size_t *out_n);
// PKCS#1 v1.5: 128 bytes in → secret out (out comporta 128). -1 falha.
int minecpp_rsa_decrypt(minecpp_rsa_t *k, const uint8_t in128[128],
                        uint8_t *out, size_t *out_n);

typedef struct minecpp_aes minecpp_aes_t;  // CFB8, um sentido, com estado

minecpp_aes_t *minecpp_aes_encrypt_init(const uint8_t key[16]);
minecpp_aes_t *minecpp_aes_decrypt_init(const uint8_t key[16]);
void minecpp_aes_update(minecpp_aes_t *a, const uint8_t *in, uint8_t *out,
                        size_t n);
void minecpp_aes_free(minecpp_aes_t *a);

// SHA1 (online-mode server hash; deferred). 0 ok, -1 sem OpenSSL.
int minecpp_sha1(const uint8_t *data, size_t len, uint8_t out[20]);

#ifdef __cplusplus
}
#endif
