#pragma once

// NBT (Named Binary Tag) — formato vanilla 1.8, extraído de `gd` (NBTBase)
// + tags `fr/fm/gb/fu/fw/fs/fq/fl/gc/fv/fn/ft`. Ver docs/vanilla-1.8.md §2.
//
// Regras:
// - Tudo big-endian. String = writeUTF (u16 len + modified UTF-8).
// - Este módulo trabalha em buffers crus (sem gzip/zlib, sem arquivo).
//   Compressão é da camada de I/O; aqui só parse/serialize determinístico.
// - Parse preserva ordem dos filhos → serialize(parse(x)) == x byte a byte.
// - Strings/nomes guardam os bytes crus (round-trip idêntico mesmo com CESU-8).

#include <stddef.h>
#include <stdint.h>

typedef enum minecpp_nbt_type {
  MINECPP_NBT_END = 0,
  MINECPP_NBT_BYTE = 1,
  MINECPP_NBT_SHORT = 2,
  MINECPP_NBT_INT = 3,
  MINECPP_NBT_LONG = 4,
  MINECPP_NBT_FLOAT = 5,
  MINECPP_NBT_DOUBLE = 6,
  MINECPP_NBT_BYTE_ARRAY = 7,
  MINECPP_NBT_STRING = 8,
  MINECPP_NBT_LIST = 9,
  MINECPP_NBT_COMPOUND = 10,
  MINECPP_NBT_INT_ARRAY = 11,
} minecpp_nbt_type_t;

typedef enum minecpp_nbt_err {
  MINECPP_NBT_OK = 0,
  MINECPP_NBT_TRUNCATED,  // buffer acabou no meio do payload
  MINECPP_NBT_BAD_ID,     // tag id > 11, ou root END, ou lista de END não-vazia
  MINECPP_NBT_BAD_DATA,   // estrutura inválida (ex: string > 65535, count negativo)
  MINECPP_NBT_TOO_DEEP,   // aninhamento > 512 (limite vanilla)
  MINECPP_NBT_OVER_BUDGET,  // estourou max_bytes (anti-OOM em input hostil)
  MINECPP_NBT_NOMEM,
} minecpp_nbt_err_t;

typedef struct minecpp_nbt_tag minecpp_nbt_tag_t;

struct minecpp_nbt_tag {
  minecpp_nbt_type_t type;
  char *name;  // owned, NUL-terminado (bytes crus); NULL em elementos de lista
  uint16_t name_len;  // tamanho em bytes (sem NUL)
  union {
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
    struct {
      uint8_t *data;
      size_t len;
    } bytes;  // BYTE_ARRAY
    struct {
      uint8_t *data;  // bytes crus modified-UTF-8 + NUL de conveniência
      uint16_t len;
    } text;  // STRING
    struct {
      int32_t *data;
      size_t len;
    } ints;  // INT_ARRAY
    struct {
      minecpp_nbt_type_t elem;  // END(0) se vazia
      minecpp_nbt_tag_t **items;
      size_t len;
    } list;
    struct {
      minecpp_nbt_tag_t **items;  // ordem de parse preservada
      size_t len;
    } compound;
  } v;
};

#define MINECPP_NBT_DEFAULT_BUDGET (64u * 1024u * 1024u)
#define MINECPP_NBT_MAX_DEPTH 512

#ifdef __cplusplus
extern "C" {
#endif

const char *minecpp_nbt_type_name(minecpp_nbt_type_t t);
const char *minecpp_nbt_err_name(minecpp_nbt_err_t e);

// Parse UMA tag nomeada (root). max_bytes=0 usa DEFAULT_BUDGET.
// Em sucesso: *out é dona da árvore (liberar com _free), *consumed = bytes lidos.
minecpp_nbt_err_t minecpp_nbt_parse(const uint8_t *data, size_t len,
                                    size_t max_bytes,
                                    minecpp_nbt_tag_t **out,
                                    size_t *consumed);

// Serializa a árvore. Aloca *out com tamanho exato *out_len (free() nela).
minecpp_nbt_err_t minecpp_nbt_serialize(const minecpp_nbt_tag_t *root,
                                        uint8_t **out, size_t *out_len);

void minecpp_nbt_free(minecpp_nbt_tag_t *tag);

// Constrói tag zerada do tipo (name pode ser NULL → sem nome). Para testes/builders.
minecpp_nbt_tag_t *minecpp_nbt_new(minecpp_nbt_type_t type, const char *name);
// Anexa child (rouba ownership). compound: exige child com nome. list: exige sem
// nome e do mesmo elem (ou lista vazia, que adota o tipo). Retorna 0 ou -1 (NOMEM).
int minecpp_nbt_add(minecpp_nbt_tag_t *parent, minecpp_nbt_tag_t *child);

// Busca filho de compound por nome (NULL se não for compound ou não achar).
const minecpp_nbt_tag_t *minecpp_nbt_get(const minecpp_nbt_tag_t *compound,
                                         const char *name);

// Remove filho de compound e devolve ownership SEM liberar (NULL se ausente).
// Usado para preservar subárvores verbatim (ex: Entities do chunk).
minecpp_nbt_tag_t *minecpp_nbt_detach(minecpp_nbt_tag_t *compound,
                                      const char *name);

// Clona subárvore inteira (NULL entra, NULL sai). NULL=NOMEM.
minecpp_nbt_tag_t *minecpp_nbt_clone(const minecpp_nbt_tag_t *s);

// Igualdade semântica: compounds comparam sem ordem; listas com ordem;
// bytes e escalares exatos. Para comparar com arquivos vanilla, cuja ordem
// de chaves é hash-order do HashMap (ver docs/vanilla-1.8.md §6).
int minecpp_nbt_equal(const minecpp_nbt_tag_t *a, const minecpp_nbt_tag_t *b);

#ifdef __cplusplus
}
#endif
