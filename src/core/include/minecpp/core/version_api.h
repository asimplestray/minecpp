#pragma once

#include <stddef.h>
#include <stdint.h>

// Buffer zero-copy do core. Dono explicito, sem malloc escondido.
typedef struct minecpp_buffer {
  uint8_t *data;
  size_t len;
  size_t cap;
} minecpp_buffer_t;

// Contrato que TODA versao implementa. Core so fala com versao por aqui.
// v1_9 herda copiando a struct da v1_8 e sobrescrevendo o que mudou.
// Ver docs/architecture.md secao 3.
typedef struct minecpp_version_api {
  int protocol_version;      // 47 = 1.8.x, 110 = 1.9.x, 210 = 1.10.x
  const char *version_name;  // "1.8.x"

  // --- chunks (opaco; implementado por cada versão em C++) ---
  // create/free gerenciam o objeto. from consome bytes NBT root
  // (ex: payload de region); to produz bytes NBT root em *out
  // (out->data via malloc, liberar com free()). Retornos: 0 ok, <0 erro.
  void *(*chunk_create)(int32_t x, int32_t z);
  void (*chunk_free)(void *chunk);
  int (*chunk_from_nbt)(void *chunk, const uint8_t *data, size_t len);
  int (*chunk_to_nbt)(const void *chunk, minecpp_buffer_t *out);
  // Blocos: id 0..4095. get retorna id (<0 fora do chunk); meta opcional.
  int32_t (*block_get)(const void *chunk, int x, int y, int z, int32_t *meta);
  int (*block_set)(void *chunk, int x, int y, int z, int32_t id, int32_t meta);
} minecpp_version_api_t;
