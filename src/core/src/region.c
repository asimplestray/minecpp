// Feature-test macros ANTES de qualquer include (glibc trava features.h na
// primeira system header — nem a nossa própria region.h pode vir antes).
#if !defined(_WIN32) && !defined(_WIN64)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L  // fseeko/ftello/off_t no strict C17
#endif
#endif

#include "minecpp/core/region.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#define minecpp_fseek _fseeki64
#define minecpp_ftell _ftelli64
typedef __int64 minecpp_off_t;
#else
#include <sys/types.h>
#define minecpp_fseek fseeko
#define minecpp_ftell ftello
typedef off_t minecpp_off_t;
#endif

struct minecpp_region {
  FILE *f;
  uint32_t off[1024];
  uint32_t ts[1024];
  uint8_t *used;  // 1 byte por setor (simples; nsectors é pequeno)
  size_t nsectors;
};

const char *minecpp_region_err_name(minecpp_region_err_t e) {
  switch (e) {
    case MINECPP_REGION_OK: return "ok";
    case MINECPP_REGION_IO: return "io";
    case MINECPP_REGION_EMPTY: return "empty";
    case MINECPP_REGION_CORRUPT: return "corrupt";
    case MINECPP_REGION_BAD_ARG: return "bad-arg";
    case MINECPP_REGION_NOMEM: return "nomem";
    case MINECPP_REGION_BAD_VERSION: return "bad-version";
    default: return "?";
  }
}

static int coords_ok(int x, int z) {
  return x >= 0 && x < MINECPP_REGION_SIZE && z >= 0 && z < MINECPP_REGION_SIZE;
}

static uint32_t rd32(const uint8_t *b) {
  return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 |
         b[3];
}

static void wr32(uint8_t *b, uint32_t v) {
  b[0] = (uint8_t)(v >> 24);
  b[1] = (uint8_t)(v >> 16);
  b[2] = (uint8_t)(v >> 8);
  b[3] = (uint8_t)v;
}

// Escreve header completo (offsets + timestamps) no início. Layout em disco
// idêntico ao vanilla (que escreve as entradas pontualmente — mesmo resultado).
static int flush_header(minecpp_region_t *r) {
  uint8_t hdr[8192];
  for (int i = 0; i < 1024; i++) {
    wr32(hdr + i * 4, r->off[i]);
    wr32(hdr + 4096 + i * 4, r->ts[i]);
  }
  if (minecpp_fseek(r->f, 0, SEEK_SET) != 0) return -1;
  return fwrite(hdr, 1, sizeof hdr, r->f) == sizeof hdr ? 0 : -1;
}

static int grow_sectors(minecpp_region_t *r, size_t need_total) {
  if (need_total <= r->nsectors) return 0;
  uint8_t *nu = (uint8_t *)realloc(r->used, need_total);
  if (!nu) return -1;
  memset(nu + r->nsectors, 0, need_total - r->nsectors);
  r->used = nu;
  r->nsectors = need_total;
  return 0;
}

minecpp_region_t *minecpp_region_open(const char *path, int create,
                                      minecpp_region_err_t *err) {
  minecpp_region_err_t e = MINECPP_REGION_OK;
  minecpp_region_t *r = NULL;
  FILE *f = NULL;
  uint8_t hdr[8192];

#define FAIL(code) \
  do {             \
    e = (code);    \
    goto fail;     \
  } while (0)

  if (!path) FAIL(MINECPP_REGION_BAD_ARG);
  r = (minecpp_region_t *)calloc(1, sizeof *r);
  if (!r) FAIL(MINECPP_REGION_NOMEM);

  f = fopen(path, "r+b");
  if (!f) {
    if (!create) FAIL(MINECPP_REGION_IO);
    f = fopen(path, "w+b");
    if (!f) FAIL(MINECPP_REGION_IO);
  }
  r->f = f;

  // Tamanho atual; normaliza p/ >= 8192 e múltiplo de 4096 (vanilla bfv faz
  // o mesmo: completa header + padding com zero).
  if (minecpp_fseek(f, 0, SEEK_END) != 0) FAIL(MINECPP_REGION_IO);
  minecpp_off_t size = minecpp_ftell(f);
  if (size < 0) FAIL(MINECPP_REGION_IO);
  size_t usize = (size_t)size;
  size_t want = usize < 8192 ? 8192 : usize;
  if (want % 4096) want += 4096 - (want % 4096);
  if (want > usize) {
    static const uint8_t zeros[4096] = {0};
    size_t missing = want - usize;
    while (missing) {
      size_t w = missing > sizeof zeros ? sizeof zeros : missing;
      if (fwrite(zeros, 1, w, f) != w) FAIL(MINECPP_REGION_IO);
      missing -= w;
    }
  }
  r->nsectors = want / 4096;
  r->used = (uint8_t *)calloc(r->nsectors, 1);
  if (!r->used) FAIL(MINECPP_REGION_NOMEM);

  if (minecpp_fseek(f, 0, SEEK_SET) != 0) FAIL(MINECPP_REGION_IO);
  if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr) FAIL(MINECPP_REGION_IO);
  for (int i = 0; i < 1024; i++) {
    r->off[i] = rd32(hdr + i * 4);
    r->ts[i] = rd32(hdr + 4096 + i * 4);
  }

  // Bitmap de setores: 0,1 = header. Entradas fora do arquivo são ignoradas
  // aqui (read reporta CORRUPT) para não corromper o mapa.
  r->used[0] = r->used[1] = 1;
  for (int i = 0; i < 1024; i++) {
    uint32_t v = r->off[i];
    if (!v) continue;
    size_t sector = v >> 8, count = v & 0xFF;
    if (!count || sector < 2 || sector + count > r->nsectors) continue;
    memset(r->used + sector, 1, count);
  }

  if (err) *err = MINECPP_REGION_OK;
  return r;

fail:
  if (r) {
    free(r->used);
    if (r->f) fclose(r->f);
    free(r);
  } else if (f) {
    fclose(f);
  }
  if (err) *err = e;
  return NULL;
#undef FAIL
}

void minecpp_region_close(minecpp_region_t *r) {
  if (!r) return;
  if (r->f) {
    flush_header(r);  // best-effort; sem o que reportar no close void
    fclose(r->f);
  }
  free(r->used);
  free(r);
}

unsigned minecpp_region_chunk_count(const minecpp_region_t *r) {
  unsigned n = 0;
  if (!r) return 0;
  for (int i = 0; i < 1024; i++)
    if (r->off[i]) n++;
  return n;
}

uint32_t minecpp_region_timestamp(const minecpp_region_t *r, int x, int z) {
  if (!r || !coords_ok(x, z)) return 0;
  return r->ts[(uint32_t)z * 32 + (uint32_t)x];
}

minecpp_region_err_t minecpp_region_read_raw(minecpp_region_t *r, int x, int z,
                                             uint8_t *out_version,
                                             uint8_t **out, size_t *out_len) {
  uint8_t head[5];
  uint32_t v, sector, count, len;

  if (!r || !out || !out_len || !out_version) return MINECPP_REGION_BAD_ARG;
  if (!coords_ok(x, z)) return MINECPP_REGION_BAD_ARG;
  v = r->off[(uint32_t)z * 32 + (uint32_t)x];
  if (!v) return MINECPP_REGION_EMPTY;
  sector = v >> 8;
  count = v & 0xFF;
  // Validação espelhada em bfv: setor dentro do arquivo, count > 0.
  if (!count || sector < 2 || (size_t)sector + count > r->nsectors)
    return MINECPP_REGION_CORRUPT;
  if (minecpp_fseek(r->f, (minecpp_off_t)sector * 4096, SEEK_SET) != 0)
    return MINECPP_REGION_IO;
  if (fread(head, 1, sizeof head, r->f) != sizeof head)
    return MINECPP_REGION_IO;
  len = rd32(head);
  // bfv: len > 4096*count → chunk inválido (retorna null). len<2 nem tem
  // version+1 byte de payload.
  if (len > count * 4096 || len < 2) return MINECPP_REGION_CORRUPT;
  if (head[4] != 1 && head[4] != 2) return MINECPP_REGION_BAD_VERSION;
  uint8_t *buf = (uint8_t *)malloc(len - 1);
  if (!buf) return MINECPP_REGION_NOMEM;
  if (fread(buf, 1, len - 1, r->f) != len - 1) {
    free(buf);
    return MINECPP_REGION_IO;
  }
  *out_version = head[4];
  *out = buf;
  *out_len = len - 1;
  return MINECPP_REGION_OK;
}

minecpp_region_err_t minecpp_region_write_raw(minecpp_region_t *r, int x, int z,
                                              uint8_t version,
                                              const uint8_t *data,
                                              size_t len) {
  uint32_t idx, old, osector, ocount;
  size_t need, total;

  if (!r || (!data && len)) return MINECPP_REGION_BAD_ARG;
  if (!coords_ok(x, z)) return MINECPP_REGION_BAD_ARG;
  if (version != 1 && version != 2) return MINECPP_REGION_BAD_ARG;
  if (len > 0xFFFFFFFEu) return MINECPP_REGION_BAD_ARG;  // len+1 cabe no u32
  total = len + 5;  // u32 len + u8 version + payload
  need = (total + 4095) / 4096;
  if (!need) need = 1;

  idx = (uint32_t)z * 32 + (uint32_t)x;
  old = r->off[idx];
  osector = old >> 8;
  ocount = old & 0xFF;

  size_t at;
  if (old && ocount >= need && osector >= 2 &&
      (size_t)osector + ocount <= r->nsectors) {
    at = osector;  // reusa no lugar (caminho quente do re-save)
  } else {
    // Libera antigo e procura run contíguo (first-fit a partir do setor 2).
    if (old && ocount && osector >= 2 &&
        (size_t)osector + ocount <= r->nsectors)
      memset(r->used + osector, 0, ocount);
    at = 0;
    for (size_t s = 2; s + need <= r->nsectors; s++) {
      size_t k = 0;
      while (k < need && !r->used[s + k]) k++;
      if (k == need) {
        at = s;
        break;
      }
      s += k;  // pula o trecho ocupado
    }
    if (!at) {  // anexa no fim
      at = r->nsectors;
      if (grow_sectors(r, r->nsectors + need) != 0)
        return MINECPP_REGION_NOMEM;
    }
    memset(r->used + at, 1, need);
    r->off[idx] = (uint32_t)((at << 8) | (need & 0xFF));
  }

  if (minecpp_fseek(r->f, (minecpp_off_t)at * 4096, SEEK_SET) != 0)
    return MINECPP_REGION_IO;
  uint8_t head[5];
  wr32(head, (uint32_t)(len + 1));
  head[4] = version;
  if (fwrite(head, 1, sizeof head, r->f) != sizeof head)
    return MINECPP_REGION_IO;
  if (len && fwrite(data, 1, len, r->f) != len) return MINECPP_REGION_IO;
  // Padding zero até fechar os setores (arquivo sempre múltiplo de 4096).
  size_t pad = need * 4096 - total;
  static const uint8_t zeros[4096] = {0};
  while (pad) {
    size_t w = pad > sizeof zeros ? sizeof zeros : pad;
    if (fwrite(zeros, 1, w, r->f) != w) return MINECPP_REGION_IO;
    pad -= w;
  }
  r->ts[idx] = (uint32_t)time(NULL);
  if (flush_header(r) != 0) return MINECPP_REGION_IO;
  return MINECPP_REGION_OK;
}
