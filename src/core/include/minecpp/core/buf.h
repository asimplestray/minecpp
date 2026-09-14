#pragma once

// Codec de protocolo — primitivas de serialização do wire format vanilla,
// extraídas de `hd` (PacketBuffer) + `dt` (BlockPos). Ver docs/vanilla-1.8.md.
// - VarInt/VarLong com os mesmos limites ("too big" em 6/11 bytes).
// - String = VarInt nbytes + UTF-8, teto nbytes <= max_chars*4 (leitura),
//   32767 bytes na escrita.
// - Position u64: x:26 (bits 38..63), y:12 (26..37), z:26 (0..25), signed.
// Agnostic de versão (idêntico 1.8→1.12); pacotes ficam em versions/vX_Y.

#include <stddef.h>
#include <stdint.h>

typedef enum minecpp_buf_err {
  MINECPP_BUF_OK = 0,
  MINECPP_BUF_TRUNCATED,
  MINECPP_BUF_OVERFLOW,  // VarInt/VarLong além do limite
  MINECPP_BUF_BAD_VALUE,  // len negativo, string acima do teto, etc.
  MINECPP_BUF_NOMEM,
} minecpp_buf_err_t;

typedef struct minecpp_reader {
  const uint8_t *p;
  size_t left;
} minecpp_reader_t;

typedef struct minecpp_writer {
  uint8_t *buf;
  size_t len, cap;
  int oom;
} minecpp_writer_t;

#ifdef __cplusplus
extern "C" {
#endif

const char *minecpp_buf_err_name(minecpp_buf_err_t e);

// --- leitura (cursor avança; erro não avança além do consumido válido) ---
minecpp_buf_err_t minecpp_rd_u8(minecpp_reader_t *r, uint8_t *v);
minecpp_buf_err_t minecpp_rd_u16(minecpp_reader_t *r, uint16_t *v);
minecpp_buf_err_t minecpp_rd_u32(minecpp_reader_t *r, uint32_t *v);
minecpp_buf_err_t minecpp_rd_u64(minecpp_reader_t *r, uint64_t *v);
minecpp_buf_err_t minecpp_rd_i32(minecpp_reader_t *r, int32_t *v);
minecpp_buf_err_t minecpp_rd_i64(minecpp_reader_t *r, int64_t *v);
minecpp_buf_err_t minecpp_rd_f32(minecpp_reader_t *r, float *v);
minecpp_buf_err_t minecpp_rd_f64(minecpp_reader_t *r, double *v);
minecpp_buf_err_t minecpp_rd_bool(minecpp_reader_t *r, int *v);
minecpp_buf_err_t minecpp_rd_varint(minecpp_reader_t *r, int32_t *v);
minecpp_buf_err_t minecpp_rd_varlong(minecpp_reader_t *r, int64_t *v);
// String UTF-8 (malloc + NUL; *out_len = bytes sem NUL). max_chars = teto.
minecpp_buf_err_t minecpp_rd_string(minecpp_reader_t *r, int max_chars,
                                    char **out, int *out_len);
// Position vanilla 1.8 (x,y,z signed).
minecpp_buf_err_t minecpp_rd_pos(minecpp_reader_t *r, int32_t *x, int32_t *y,
                                 int32_t *z);
minecpp_buf_err_t minecpp_rd_raw(minecpp_reader_t *r, void *dst, size_t n);

// --- escrita (growable; oom retido em w, checar via minecpp_wr_ok) ---
void minecpp_wr_u8(minecpp_writer_t *w, uint8_t v);
void minecpp_wr_u16(minecpp_writer_t *w, uint16_t v);
void minecpp_wr_u32(minecpp_writer_t *w, uint32_t v);
void minecpp_wr_u64(minecpp_writer_t *w, uint64_t v);
void minecpp_wr_i32(minecpp_writer_t *w, int32_t v);
void minecpp_wr_i64(minecpp_writer_t *w, int64_t v);
void minecpp_wr_f32(minecpp_writer_t *w, float v);
void minecpp_wr_f64(minecpp_writer_t *w, double v);
void minecpp_wr_bool(minecpp_writer_t *w, int v);
void minecpp_wr_varint(minecpp_writer_t *w, int32_t v);
void minecpp_wr_varlong(minecpp_writer_t *w, int64_t v);
void minecpp_wr_strn(minecpp_writer_t *w, const char *s, size_t n);
void minecpp_wr_str(minecpp_writer_t *w, const char *s);
void minecpp_wr_pos(minecpp_writer_t *w, int32_t x, int32_t y, int32_t z);
void minecpp_wr_raw(minecpp_writer_t *w, const void *d, size_t n);
int minecpp_wr_ok(const minecpp_writer_t *w);
// Transfere ownership do buffer (free() nele). Retorna NULL se oom/vazio?.
uint8_t *minecpp_wr_take(minecpp_writer_t *w, size_t *out_len);

#ifdef __cplusplus
}
#endif
