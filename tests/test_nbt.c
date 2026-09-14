// Testes NBT contra bytes reais do vanilla 1.8 (test-data/vanilla-1.8-flat/).
// Sem framework: asserts contam falhas, main retorna nº de falhas.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "minecpp/core/nbt.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "test-data/vanilla-1.8-flat"
#endif

static int g_fail = 0;

#define ASSERT(cond)                                                      \
  do {                                                                    \
    if (!(cond)) {                                                        \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      g_fail++;                                                           \
    }                                                                     \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static uint8_t *read_file(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("FAIL: sem arquivo %s\n", path);
    g_fail++;
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *b = (uint8_t *)malloc((size_t)n ? (size_t)n : 1);
  if (fread(b, 1, (size_t)n, f) != (size_t)n) {
    printf("FAIL: leitura %s\n", path);
    g_fail++;
    fclose(f);
    free(b);
    return NULL;
  }
  fclose(f);
  *len = (size_t)n;
  return b;
}

static uint8_t *inflate_with(const uint8_t *in, size_t in_len, size_t *out_len,
                              int window) {
  size_t cap = in_len * 4 + 64;
  uint8_t *out = (uint8_t *)malloc(cap);
  z_stream s;
  memset(&s, 0, sizeof s);
  if (inflateInit2(&s, window) != Z_OK) {
    free(out);
    return NULL;
  }
  s.next_in = (Bytef *)in;
  s.avail_in = (uInt)in_len;
  for (;;) {
    if (s.total_out >= cap) {
      cap *= 2;
      uint8_t *nb = (uint8_t *)realloc(out, cap);
      if (!nb) {
        free(out);
        inflateEnd(&s);
        return NULL;
      }
      out = nb;
    }
    s.next_out = out + s.total_out;
    s.avail_out = (uInt)(cap - s.total_out);
    int r = inflate(&s, Z_NO_FLUSH);
    if (r == Z_STREAM_END) break;
    if (r != Z_OK) {
      free(out);
      inflateEnd(&s);
      return NULL;
    }
  }
  *out_len = s.total_out;
  inflateEnd(&s);
  return out;
}

static uint8_t *gunzip(const uint8_t *in, size_t in_len, size_t *out_len) {
  // level.dat = gzip; chunk region v2 = zlib. Tenta gzip, cai pra zlib.
  uint8_t *out = inflate_with(in, in_len, out_len, 16 + MAX_WBITS);
  if (!out) out = inflate_with(in, in_len, out_len, MAX_WBITS);
  return out;
}

// t1: todos os 12 tipos em memória, round-trip preserva bytes.
static void t_mem_all_types(void) {
  minecpp_nbt_tag_t *root = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "");
  ASSERT(root);
  minecpp_nbt_tag_t *c;
  c = minecpp_nbt_new(MINECPP_NBT_BYTE, "b");
  c->v.i8 = -5;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_SHORT, "s");
  c->v.i16 = -300;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_INT, "i");
  c->v.i32 = 19133;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_LONG, "l");
  c->v.i64 = -1234567890123LL;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_FLOAT, "f");
  c->v.f32 = 3.14f;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_DOUBLE, "d");
  c->v.f64 = -0.001;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_BYTE_ARRAY, "ba");
  uint8_t ba[] = {0, 1, 2, 250, 255};
  c->v.bytes.data = (uint8_t *)malloc(sizeof ba);
  memcpy(c->v.bytes.data, ba, sizeof ba);
  c->v.bytes.len = sizeof ba;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_BYTE_ARRAY, "ba0");  // vazio
  c->v.bytes.data = NULL;
  c->v.bytes.len = 0;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_STRING, "str");
  const char *sv = "flat";
  c->v.text.data = (uint8_t *)malloc(5);
  memcpy(c->v.text.data, sv, 5);
  c->v.text.len = 4;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_STRING, "str0");  // vazia
  c->v.text.data = (uint8_t *)malloc(1);
  c->v.text.data[0] = 0;
  c->v.text.len = 0;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_INT_ARRAY, "ia");
  int32_t ia[] = {1, -2, 2147483647};
  c->v.ints.data = (int32_t *)malloc(sizeof ia);
  memcpy(c->v.ints.data, ia, sizeof ia);
  c->v.ints.len = 3;
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  minecpp_nbt_tag_t *list = minecpp_nbt_new(MINECPP_NBT_LIST, "li");
  for (int i = 0; i < 3; i++) {
    c = minecpp_nbt_new(MINECPP_NBT_INT, NULL);
    c->v.i32 = i * 10;
    ASSERT_EQ(minecpp_nbt_add(list, c), 0);
  }
  ASSERT_EQ(minecpp_nbt_add(root, list), 0);
  c = minecpp_nbt_new(MINECPP_NBT_LIST, "le");  // lista vazia
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);
  minecpp_nbt_tag_t *sub = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "sub");
  c = minecpp_nbt_new(MINECPP_NBT_BYTE, "y");
  c->v.i8 = 4;
  ASSERT_EQ(minecpp_nbt_add(sub, c), 0);
  ASSERT_EQ(minecpp_nbt_add(root, sub), 0);
  c = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "ce");  // compound vazio
  ASSERT_EQ(minecpp_nbt_add(root, c), 0);

  uint8_t *b1 = NULL;
  size_t n1 = 0;
  ASSERT_EQ(minecpp_nbt_serialize(root, &b1, &n1), MINECPP_NBT_OK);
  minecpp_nbt_tag_t *p = NULL;
  size_t used = 0;
  ASSERT_EQ(minecpp_nbt_parse(b1, n1, 0, &p, &used), MINECPP_NBT_OK);
  ASSERT_EQ(used, n1);
  const minecpp_nbt_tag_t *q = minecpp_nbt_get(p, "i");
  ASSERT(q && q->type == MINECPP_NBT_INT && q->v.i32 == 19133);
  q = minecpp_nbt_get(p, "str");
  ASSERT(q && q->type == MINECPP_NBT_STRING && q->v.text.len == 4 &&
         memcmp(q->v.text.data, "flat", 4) == 0);
  q = minecpp_nbt_get(p, "li");
  ASSERT(q && q->type == MINECPP_NBT_LIST && q->v.list.len == 3 &&
         q->v.list.elem == MINECPP_NBT_INT);
  q = minecpp_nbt_get(p, "le");
  ASSERT(q && q->type == MINECPP_NBT_LIST && q->v.list.len == 0 &&
         q->v.list.elem == MINECPP_NBT_END);
  uint8_t *b2 = NULL;
  size_t n2 = 0;
  ASSERT_EQ(minecpp_nbt_serialize(p, &b2, &n2), MINECPP_NBT_OK);
  ASSERT_EQ(n1, n2);
  ASSERT(n1 > 0 && memcmp(b1, b2, n1) == 0);
  free(b1);
  free(b2);
  minecpp_nbt_free(p);
  minecpp_nbt_free(root);
}

// t2: level.dat real — parse, valores, round-trip byte-idêntico.
static void t_level_real(void) {
  size_t gz_len = 0;
  uint8_t *gz = read_file(TEST_DATA_DIR "/level.dat", &gz_len);
  if (!gz) return;
  size_t raw_len = 0;
  uint8_t *raw = gunzip(gz, gz_len, &raw_len);
  free(gz);
  ASSERT(raw);
  if (!raw) return;
  ASSERT_EQ(raw_len, 1029u);
  ASSERT_EQ(raw[0], 10);  // TAG_Compound root

  minecpp_nbt_tag_t *root = NULL;
  size_t used = 0;
  minecpp_nbt_err_t e = minecpp_nbt_parse(raw, raw_len, 0, &root, &used);
  ASSERT_EQ(e, MINECPP_NBT_OK);
  if (!root) {
    free(raw);
    return;
  }
  ASSERT_EQ(used, raw_len);
  ASSERT(root->type == MINECPP_NBT_COMPOUND && root->name_len == 0);
  const minecpp_nbt_tag_t *data = minecpp_nbt_get(root, "Data");
  ASSERT(data && data->type == MINECPP_NBT_COMPOUND);
  const minecpp_nbt_tag_t *gen = minecpp_nbt_get(data, "generatorName");
  ASSERT(gen && gen->type == MINECPP_NBT_STRING && gen->v.text.len == 4 &&
         memcmp(gen->v.text.data, "flat", 4) == 0);
  const minecpp_nbt_tag_t *ver = minecpp_nbt_get(data, "version");
  ASSERT(ver && ver->type == MINECPP_NBT_INT && ver->v.i32 == 19133);
  const minecpp_nbt_tag_t *sx = minecpp_nbt_get(data, "SpawnX");
  ASSERT(sx && sx->type == MINECPP_NBT_INT);

  uint8_t *back = NULL;
  size_t back_len = 0;
  ASSERT_EQ(minecpp_nbt_serialize(root, &back, &back_len), MINECPP_NBT_OK);
  ASSERT_EQ(back_len, raw_len);
  ASSERT(back_len > 0 && memcmp(back, raw, raw_len) == 0);
  free(back);
  minecpp_nbt_free(root);
  free(raw);
}

// t3: chunk real do r.-1.-1.mca — header, zlib v2, Level/Sections, round-trip.
static void t_chunk_real(void) {
  size_t f_len = 0;
  uint8_t *f =
      read_file(TEST_DATA_DIR "/region/r.-1.-1.mca", &f_len);
  if (!f) return;
  ASSERT_EQ(f_len, 16384u);
  // Header: 1024 u32 BE offsets.
  uint32_t slot = 0, sector = 0, count = 0;
  int found = 0, sx = 0, sz = 0;
  for (int i = 0; i < 1024; i++) {
    uint32_t v = (uint32_t)f[i * 4] << 24 | (uint32_t)f[i * 4 + 1] << 16 |
                 (uint32_t)f[i * 4 + 2] << 8 | f[i * 4 + 3];
    if (v) {
      slot = (uint32_t)i;
      sector = v >> 8;
      count = v & 0xFF;
      sx = i % 32;
      sz = i / 32;
      found = 1;
      break;
    }
  }
  ASSERT(found);
  ASSERT_EQ(sx, 31);
  ASSERT_EQ(sz, 20);
  size_t base = (size_t)sector * 4096;
  uint32_t len = (uint32_t)f[base] << 24 | (uint32_t)f[base + 1] << 16 |
                 (uint32_t)f[base + 2] << 8 | f[base + 3];
  ASSERT_EQ(f[base + 4], 2);  // version 2 = zlib (vanilla 1.8 escreve 2)
  (void)count;
  (void)slot;
  uLongf dst_len = 0;
  uint8_t *nbt_raw = gunzip(f + base + 5, len - 1, (size_t *)&dst_len);
  ASSERT(nbt_raw);
  free(f);
  if (!nbt_raw) return;
  ASSERT(nbt_raw[0] == 10);

  minecpp_nbt_tag_t *root = NULL;
  size_t used = 0;
  ASSERT_EQ(minecpp_nbt_parse(nbt_raw, dst_len, 0, &root, &used),
            MINECPP_NBT_OK);
  if (!root) {
    free(nbt_raw);
    return;
  }
  ASSERT_EQ(used, (size_t)dst_len);
  const minecpp_nbt_tag_t *level = minecpp_nbt_get(root, "Level");
  ASSERT(level && level->type == MINECPP_NBT_COMPOUND);
  const minecpp_nbt_tag_t *xp = minecpp_nbt_get(level, "xPos");
  const minecpp_nbt_tag_t *zp = minecpp_nbt_get(level, "zPos");
  ASSERT(xp && xp->v.i32 == -1);  // slot(31,20) região(-1,-1) → chunk(-1,-12)
  ASSERT(zp && zp->v.i32 == -12);
  const minecpp_nbt_tag_t *sec = minecpp_nbt_get(level, "Sections");
  ASSERT(sec && sec->type == MINECPP_NBT_LIST && sec->v.list.len > 0);
  const minecpp_nbt_tag_t *s0 = sec->v.list.items[0];
  const minecpp_nbt_tag_t *blocks = minecpp_nbt_get(s0, "Blocks");
  const minecpp_nbt_tag_t *data = minecpp_nbt_get(s0, "Data");
  ASSERT(blocks && blocks->type == MINECPP_NBT_BYTE_ARRAY &&
         blocks->v.bytes.len == 4096);
  ASSERT(data && data->type == MINECPP_NBT_BYTE_ARRAY &&
         data->v.bytes.len == 2048);

  uint8_t *back = NULL;
  size_t back_len = 0;
  ASSERT_EQ(minecpp_nbt_serialize(root, &back, &back_len), MINECPP_NBT_OK);
  ASSERT_EQ(back_len, (size_t)dst_len);
  ASSERT(memcmp(back, nbt_raw, dst_len) == 0);
  free(back);
  minecpp_nbt_free(root);
  free(nbt_raw);
}

// t4: inputs hostis.
static void t_corrupt(void) {
  minecpp_nbt_tag_t *out = NULL;
  size_t used = 0;
  uint8_t trunc[] = {10, 0, 0, 1, 'a'};  // nome "a"?? corta no meio
  ASSERT_EQ(minecpp_nbt_parse(trunc, sizeof trunc, 0, &out, &used),
            MINECPP_NBT_TRUNCATED);
  uint8_t badid[] = {12, 0, 0};
  ASSERT_EQ(minecpp_nbt_parse(badid, sizeof badid, 0, &out, &used),
            MINECPP_NBT_BAD_ID);
  uint8_t endlist[] = {10, 0, 0, 9, 0, 1, 'L', 0, 0, 0, 0, 2, 0};
  // lista elem END(0) com count=2 → BAD_DATA
  ASSERT_EQ(minecpp_nbt_parse(endlist, sizeof endlist, 0, &out, &used),
            MINECPP_NBT_BAD_DATA);
  // budget minúsculo num buffer válido
  uint8_t ok[] = {10, 0, 0, 1, 0, 1, 'a', 7, 0};
  ASSERT_EQ(minecpp_nbt_parse(ok, sizeof ok, 10, &out, &used),
            MINECPP_NBT_OVER_BUDGET);
  // profundidade > 512
  minecpp_nbt_tag_t *deep = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "");
  minecpp_nbt_tag_t *cur = deep;
  for (int i = 0; i < 600; i++) {
    minecpp_nbt_tag_t *n = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "n");
    minecpp_nbt_add(cur, n);
    cur = n;
  }
  uint8_t *db = NULL;
  size_t dn = 0;
  ASSERT_EQ(minecpp_nbt_serialize(deep, &db, &dn), MINECPP_NBT_OK);
  ASSERT_EQ(minecpp_nbt_parse(db, dn, 0, &out, &used), MINECPP_NBT_TOO_DEEP);
  free(db);
  minecpp_nbt_free(deep);
}

// t5: equal sem ordem + detach.
static void t_equal_detach(void) {
  minecpp_nbt_tag_t *a = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "");
  minecpp_nbt_tag_t *b = minecpp_nbt_new(MINECPP_NBT_COMPOUND, "");
  minecpp_nbt_tag_t *c;
  c = minecpp_nbt_new(MINECPP_NBT_INT, "x");
  c->v.i32 = 1;
  ASSERT_EQ(minecpp_nbt_add(a, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_STRING, "y");
  c->v.text.data = (uint8_t *)malloc(2);
  memcpy(c->v.text.data, "s", 2);
  c->v.text.len = 1;
  ASSERT_EQ(minecpp_nbt_add(a, c), 0);
  // b na ordem inversa, mesmo conteúdo.
  c = minecpp_nbt_new(MINECPP_NBT_STRING, "y");
  c->v.text.data = (uint8_t *)malloc(2);
  memcpy(c->v.text.data, "s", 2);
  c->v.text.len = 1;
  ASSERT_EQ(minecpp_nbt_add(b, c), 0);
  c = minecpp_nbt_new(MINECPP_NBT_INT, "x");
  c->v.i32 = 1;
  ASSERT_EQ(minecpp_nbt_add(b, c), 0);
  ASSERT(minecpp_nbt_equal(a, b));
  ASSERT(minecpp_nbt_equal(a, a));
  // Valor diferente → desigual.
  ((minecpp_nbt_tag_t *)minecpp_nbt_get(b, "x"))->v.i32 = 2;
  ASSERT(!minecpp_nbt_equal(a, b));
  ((minecpp_nbt_tag_t *)minecpp_nbt_get(b, "x"))->v.i32 = 1;
  ASSERT(minecpp_nbt_equal(a, b));
  // Lista: ordem importa.
  minecpp_nbt_tag_t *l1 = minecpp_nbt_new(MINECPP_NBT_LIST, "l");
  minecpp_nbt_tag_t *l2 = minecpp_nbt_new(MINECPP_NBT_LIST, "l");
  for (int i = 1; i <= 2; i++) {
    c = minecpp_nbt_new(MINECPP_NBT_INT, NULL);
    c->v.i32 = i;
    ASSERT_EQ(minecpp_nbt_add(l1, c), 0);
    c = minecpp_nbt_new(MINECPP_NBT_INT, NULL);
    c->v.i32 = 3 - i;
    ASSERT_EQ(minecpp_nbt_add(l2, c), 0);
  }
  ASSERT(!minecpp_nbt_equal(l1, l2));
  minecpp_nbt_free(l1);
  minecpp_nbt_free(l2);
  // Detach transfere sem liberar.
  minecpp_nbt_tag_t *d = minecpp_nbt_detach(a, "x");
  ASSERT(d && d->type == MINECPP_NBT_INT && d->v.i32 == 1);
  ASSERT(minecpp_nbt_get(a, "x") == NULL);
  ASSERT(!minecpp_nbt_equal(a, b));
  minecpp_nbt_free(d);
  ASSERT(minecpp_nbt_detach(a, "ausente") == NULL);
  ASSERT(minecpp_nbt_detach(NULL, "x") == NULL);
  minecpp_nbt_free(a);
  minecpp_nbt_free(b);
}

int main(void) {
  t_mem_all_types();
  t_level_real();
  t_chunk_real();
  t_corrupt();
  t_equal_detach();
  if (g_fail == 0) printf("nbt: todas as verificações passaram\n");
  return g_fail ? 1 : 0;
}
