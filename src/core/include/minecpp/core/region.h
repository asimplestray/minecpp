#pragma once

// Region `.mca` — layout vanilla 1.8 extraído de `bfv` (RegionFile).
// Ver docs/vanilla-1.8.md §3.
//
// - Header 8192 B: 1024 u32 BE offsets + 1024 u32 BE timestamps.
// - Chunk em sector*4096: u32 BE len + u8 version(1=gzip,2=zlib) + payload.
// - Este módulo faz framing + setores. NÃO descomprime (sem zlib na lib):
//   read devolve bytes comprimidos, write recebe bytes comprimidos.
// - I/O síncrono no MVP (assinaturas já isolam o FILE* p/ futuro async).

#include <stddef.h>
#include <stdint.h>

#define MINECPP_REGION_SIZE 32
#define MINECPP_REGION_SECTOR 4096

typedef enum minecpp_region_err {
  MINECPP_REGION_OK = 0,
  MINECPP_REGION_IO,  // fopen/fread/fwrite falhou
  MINECPP_REGION_EMPTY,  // slot vazio (offset 0)
  MINECPP_REGION_CORRUPT,  // offset/len fora da realidade do arquivo
  MINECPP_REGION_BAD_ARG,  // coords fora de 0..31, version inválida p/ escrita
  MINECPP_REGION_NOMEM,
  MINECPP_REGION_BAD_VERSION,  // version lida != 1,2
} minecpp_region_err_t;

typedef struct minecpp_region minecpp_region_t;

#ifdef __cplusplus
extern "C" {
#endif

const char *minecpp_region_err_name(minecpp_region_err_t e);

// Abre ou (create!=0) cria .mca. Normaliza tamanho p/ múltiplo de 4096.
// Retorna NULL em falha (IO/NOMEM), com *err preenchido (pode ser NULL).
minecpp_region_t *minecpp_region_open(const char *path, int create,
                                      minecpp_region_err_t *err);
// Flush do header + fclose.
void minecpp_region_close(minecpp_region_t *r);

unsigned minecpp_region_chunk_count(const minecpp_region_t *r);
uint32_t minecpp_region_timestamp(const minecpp_region_t *r, int x, int z);

// Lê payload cru: *out_version=1|2, *out=malloc(len-1 bytes comprimidos).
minecpp_region_err_t minecpp_region_read_raw(minecpp_region_t *r, int x, int z,
                                             uint8_t *out_version,
                                             uint8_t **out, size_t *out_len);

// Escreve chunk (version 1 ou 2). Reusa setor se couber, senão realoca/anexa.
// Atualiza offset + timestamp (time(NULL)) e faz flush do header.
minecpp_region_err_t minecpp_region_write_raw(minecpp_region_t *r, int x, int z,
                                              uint8_t version,
                                              const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
