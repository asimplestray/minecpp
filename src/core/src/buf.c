#include "minecpp/core/buf.h"

#include <stdlib.h>
#include <string.h>

#define MINECPP_STR_MAX_BYTES 32767

const char *minecpp_buf_err_name(minecpp_buf_err_t e) {
  switch (e) {
    case MINECPP_BUF_OK: return "ok";
    case MINECPP_BUF_TRUNCATED: return "truncated";
    case MINECPP_BUF_OVERFLOW: return "overflow";
    case MINECPP_BUF_BAD_VALUE: return "bad-value";
    case MINECPP_BUF_NOMEM: return "nomem";
    default: return "?";
  }
}

// ---------------------------------------------------------------- leitura

static minecpp_buf_err_t rd_take(minecpp_reader_t *r, void *dst, size_t n) {
  if (n > r->left) return MINECPP_BUF_TRUNCATED;
  memcpy(dst, r->p, n);
  r->p += n;
  r->left -= n;
  return MINECPP_BUF_OK;
}

minecpp_buf_err_t minecpp_rd_u8(minecpp_reader_t *r, uint8_t *v) {
  return rd_take(r, v, 1);
}

minecpp_buf_err_t minecpp_rd_u16(minecpp_reader_t *r, uint16_t *v) {
  uint8_t b[2];
  minecpp_buf_err_t e = rd_take(r, b, 2);
  if (e == MINECPP_BUF_OK) *v = (uint16_t)((uint16_t)b[0] << 8 | b[1]);
  return e;
}

minecpp_buf_err_t minecpp_rd_u32(minecpp_reader_t *r, uint32_t *v) {
  uint8_t b[4];
  minecpp_buf_err_t e = rd_take(r, b, 4);
  if (e == MINECPP_BUF_OK)
    *v = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 |
         b[3];
  return e;
}

minecpp_buf_err_t minecpp_rd_u64(minecpp_reader_t *r, uint64_t *v) {
  uint8_t b[8];
  minecpp_buf_err_t e = rd_take(r, b, 8);
  if (e != MINECPP_BUF_OK) return e;
  uint64_t u = 0;
  for (int i = 0; i < 8; i++) u = u << 8 | b[i];
  *v = u;
  return MINECPP_BUF_OK;
}

minecpp_buf_err_t minecpp_rd_i32(minecpp_reader_t *r, int32_t *v) {
  uint32_t u;
  minecpp_buf_err_t e = minecpp_rd_u32(r, &u);
  if (e == MINECPP_BUF_OK) *v = (int32_t)u;
  return e;
}

minecpp_buf_err_t minecpp_rd_i64(minecpp_reader_t *r, int64_t *v) {
  uint64_t u;
  minecpp_buf_err_t e = minecpp_rd_u64(r, &u);
  if (e == MINECPP_BUF_OK) *v = (int64_t)u;
  return e;
}

minecpp_buf_err_t minecpp_rd_f32(minecpp_reader_t *r, float *v) {
  uint32_t u;
  minecpp_buf_err_t e = minecpp_rd_u32(r, &u);
  if (e == MINECPP_BUF_OK) memcpy(v, &u, 4);
  return e;
}

minecpp_buf_err_t minecpp_rd_f64(minecpp_reader_t *r, double *v) {
  uint64_t u;
  minecpp_buf_err_t e = minecpp_rd_u64(r, &u);
  if (e == MINECPP_BUF_OK) memcpy(v, &u, 8);
  return e;
}

minecpp_buf_err_t minecpp_rd_bool(minecpp_reader_t *r, int *v) {
  uint8_t b;
  minecpp_buf_err_t e = minecpp_rd_u8(r, &b);
  if (e == MINECPP_BUF_OK) *v = b != 0;
  return e;
}

// hd.e(): até 5 bytes; 6º byte com continuação = "VarInt too big".
minecpp_buf_err_t minecpp_rd_varint(minecpp_reader_t *r, int32_t *v) {
  uint32_t out = 0;
  for (int shift = 0; shift < 35; shift += 7) {
    uint8_t b;
    minecpp_buf_err_t e = minecpp_rd_u8(r, &b);
    if (e != MINECPP_BUF_OK) return e;
    out |= (uint32_t)(b & 0x7F) << shift;
    if (!(b & 0x80)) {
      *v = (int32_t)out;
      return MINECPP_BUF_OK;
    }
  }
  return MINECPP_BUF_OVERFLOW;
}

// hd.f(): até 10 bytes.
minecpp_buf_err_t minecpp_rd_varlong(minecpp_reader_t *r, int64_t *v) {
  uint64_t out = 0;
  for (int shift = 0; shift < 70; shift += 7) {
    uint8_t b;
    minecpp_buf_err_t e = minecpp_rd_u8(r, &b);
    if (e != MINECPP_BUF_OK) return e;
    out |= (uint64_t)(b & 0x7F) << shift;
    if (!(b & 0x80)) {
      *v = (int64_t)out;
      return MINECPP_BUF_OK;
    }
  }
  return MINECPP_BUF_OVERFLOW;
}

// hd.c(maxChars): VarInt nbytes, teto nbytes <= max*4, rejeita negativo.
minecpp_buf_err_t minecpp_rd_string(minecpp_reader_t *r, int max_chars,
                                    char **out, int *out_len) {
  int32_t n = 0;
  minecpp_buf_err_t e = minecpp_rd_varint(r, &n);
  if (e != MINECPP_BUF_OK) return e;
  if (n < 0) return MINECPP_BUF_BAD_VALUE;
  if (max_chars < 0 || n > max_chars * 4) return MINECPP_BUF_BAD_VALUE;
  if ((size_t)n > r->left) return MINECPP_BUF_TRUNCATED;
  char *s = (char *)malloc((size_t)n + 1);
  if (!s) return MINECPP_BUF_NOMEM;
  memcpy(s, r->p, (size_t)n);
  s[n] = '\0';
  r->p += (size_t)n;
  r->left -= (size_t)n;
  *out = s;
  if (out_len) *out_len = n;
  return MINECPP_BUF_OK;
}

// dt.a(long): x = v>>38 com sign-extend do bit 63 (26b em 38..63),
// y = v<<26>>52 (12b), z = v<<38>>38 (26b).
minecpp_buf_err_t minecpp_rd_pos(minecpp_reader_t *r, int32_t *x, int32_t *y,
                                 int32_t *z) {
  uint64_t v;
  minecpp_buf_err_t e = minecpp_rd_u64(r, &v);
  if (e != MINECPP_BUF_OK) return e;
  if (x) *x = (int32_t)((int64_t)v >> 38);
  if (y) *y = (int32_t)((int64_t)(v << 26) >> 52);
  if (z) *z = (int32_t)((int64_t)(v << 38) >> 38);
  return MINECPP_BUF_OK;
}

minecpp_buf_err_t minecpp_rd_raw(minecpp_reader_t *r, void *dst, size_t n) {
  return rd_take(r, dst, n);
}

// ---------------------------------------------------------------- escrita

static void wr_need(minecpp_writer_t *w, size_t n) {
  if (w->oom) return;
  if (w->len + n <= w->cap) return;
  size_t ncap = w->cap ? w->cap : 64;
  while (ncap < w->len + n) {
    if (ncap > (size_t)1 << 30) {
      w->oom = 1;
      return;
    }
    ncap *= 2;
  }
  uint8_t *nb = (uint8_t *)realloc(w->buf, ncap);
  if (!nb) {
    w->oom = 1;
    return;
  }
  w->buf = nb;
  w->cap = ncap;
}

void minecpp_wr_raw(minecpp_writer_t *w, const void *d, size_t n) {
  wr_need(w, n);
  if (w->oom) return;
  memcpy(w->buf + w->len, d, n);
  w->len += n;
}

void minecpp_wr_u8(minecpp_writer_t *w, uint8_t v) { minecpp_wr_raw(w, &v, 1); }

void minecpp_wr_u16(minecpp_writer_t *w, uint16_t v) {
  const uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v};
  minecpp_wr_raw(w, b, 2);
}

void minecpp_wr_u32(minecpp_writer_t *w, uint32_t v) {
  const uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16),
                        (uint8_t)(v >> 8), (uint8_t)v};
  minecpp_wr_raw(w, b, 4);
}

void minecpp_wr_u64(minecpp_writer_t *w, uint64_t v) {
  uint8_t b[8];
  for (int i = 7; i >= 0; i--) {
    b[i] = (uint8_t)v;
    v >>= 8;
  }
  minecpp_wr_raw(w, b, 8);
}

void minecpp_wr_i32(minecpp_writer_t *w, int32_t v) {
  minecpp_wr_u32(w, (uint32_t)v);
}

void minecpp_wr_i64(minecpp_writer_t *w, int64_t v) {
  minecpp_wr_u64(w, (uint64_t)v);
}

void minecpp_wr_f32(minecpp_writer_t *w, float v) {
  uint32_t u;
  memcpy(&u, &v, 4);
  minecpp_wr_u32(w, u);
}

void minecpp_wr_f64(minecpp_writer_t *w, double v) {
  uint64_t u;
  memcpy(&u, &v, 8);
  minecpp_wr_u64(w, u);
}

void minecpp_wr_bool(minecpp_writer_t *w, int v) {
  minecpp_wr_u8(w, v ? 1 : 0);
}

// hd.b(int): 7 bits LE por byte, MSB continua, >>> lógico.
void minecpp_wr_varint(minecpp_writer_t *w, int32_t v) {
  uint32_t u = (uint32_t)v;
  while ((u & 0xFFFFFF80u) != 0) {
    minecpp_wr_u8(w, (uint8_t)((u & 0x7F) | 0x80));
    u >>= 7;
  }
  minecpp_wr_u8(w, (uint8_t)u);
}

void minecpp_wr_varlong(minecpp_writer_t *w, int64_t v) {
  uint64_t u = (uint64_t)v;
  while ((u & 0xFFFFFFFFFFFFFF80ull) != 0) {
    minecpp_wr_u8(w, (uint8_t)((u & 0x7F) | 0x80));
    u >>= 7;
  }
  minecpp_wr_u8(w, (uint8_t)u);
}

// hd.a(String): UTF-8, teto 32767 bytes.
void minecpp_wr_strn(minecpp_writer_t *w, const char *s, size_t n) {
  if (n > MINECPP_STR_MAX_BYTES) {
    w->oom = 1;  // usa oom como "não representável" (caller checa wr_ok)
    return;
  }
  minecpp_wr_varint(w, (int32_t)n);
  if (n) minecpp_wr_raw(w, s, n);
}

void minecpp_wr_str(minecpp_writer_t *w, const char *s) {
  minecpp_wr_strn(w, s ? s : "", s ? strlen(s) : 0);
}

// dt.g(): (x&2^26-1)<<38 | (y&2^12-1)<<26 | (z&2^26-1).
void minecpp_wr_pos(minecpp_writer_t *w, int32_t x, int32_t y, int32_t z) {
  const uint64_t v = ((uint64_t)((uint32_t)x & 0x3FFFFFF) << 38) |
                     ((uint64_t)((uint32_t)y & 0xFFF) << 26) |
                     ((uint64_t)((uint32_t)z & 0x3FFFFFF));
  minecpp_wr_u64(w, v);
}

int minecpp_wr_ok(const minecpp_writer_t *w) { return w && !w->oom; }

uint8_t *minecpp_wr_take(minecpp_writer_t *w, size_t *out_len) {
  uint8_t *b = w->buf;
  const size_t n = w->len;
  if (w->oom) {
    free(b);
    return NULL;
  }
  w->buf = NULL;
  w->len = w->cap = 0;
  if (out_len) *out_len = n;
  return b;
}
