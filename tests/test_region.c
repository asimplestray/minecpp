// Testes region .mca contra o r.-1.-1.mca real + round-trip de escrita.
// Sem framework: asserts contam falhas.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "minecpp/core/nbt.h"
#include "minecpp/core/region.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "test-data/vanilla-1.8-flat"
#endif

static int g_fail = 0;

#define ASSERT(cond)                                         \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      g_fail++;                                              \
    }                                                        \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static void tmp_path(char *dst, size_t cap, const char *tag) {
#ifdef _WIN32
  int pid = _getpid();
#else
  int pid = (int)getpid();
#endif
  snprintf(dst, cap, "/tmp/minecpp-%s-%d.mca", tag, pid);
}

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
  if (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) {
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

static int write_file(const char *path, const uint8_t *d, size_t n) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  int ok = n == 0 || fwrite(d, 1, n, f) == n;
  fclose(f);
  return ok ? 0 : -1;
}

static uint8_t *zlib_inflate(const uint8_t *in, size_t in_len,
                             size_t *out_len) {
  size_t cap = in_len * 16 + 64;
  uint8_t *out = (uint8_t *)malloc(cap);
  z_stream s;
  memset(&s, 0, sizeof s);
  if (inflateInit(&s) != Z_OK) {
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

static void wr32be(uint8_t *b, uint32_t v) {
  b[0] = (uint8_t)(v >> 24);
  b[1] = (uint8_t)(v >> 16);
  b[2] = (uint8_t)(v >> 8);
  b[3] = (uint8_t)v;
}

// t1: abre cópia do vanilla real, lê chunk, confere via NBT.
static void t_open_real(void) {
  char tmp[256];
  tmp_path(tmp, sizeof tmp, "real");
  size_t f_len = 0;
  uint8_t *f = read_file(TEST_DATA_DIR "/region/r.-1.-1.mca", &f_len);
  if (!f) return;
  ASSERT_EQ(f_len, 16384u);
  ASSERT_EQ(write_file(tmp, f, f_len), 0);
  free(f);

  minecpp_region_err_t e = MINECPP_REGION_OK;
  minecpp_region_t *r = minecpp_region_open(tmp, 0, &e);
  ASSERT_EQ(e, MINECPP_REGION_OK);
  ASSERT(r);
  if (!r) {
    remove(tmp);
    return;
  }
  ASSERT_EQ(minecpp_region_chunk_count(r), 2u);
  ASSERT(minecpp_region_timestamp(r, 31, 20) != 0);
  ASSERT_EQ(minecpp_region_timestamp(r, 0, 0), 0u);

  uint8_t ver = 0;
  uint8_t *payload = NULL;
  size_t plen = 0;
  ASSERT_EQ(minecpp_region_read_raw(r, 31, 20, &ver, &payload, &plen),
            MINECPP_REGION_OK);
  ASSERT_EQ(ver, 2);  // zlib, como o vanilla 1.8 escreve
  ASSERT_EQ(plen, 255u);  // len field 256 = version + payload
  if (payload) {
    size_t nlen = 0;
    uint8_t *nbt = zlib_inflate(payload, plen, &nlen);
    ASSERT(nbt);
    if (nbt) {
      minecpp_nbt_tag_t *root = NULL;
      ASSERT_EQ(minecpp_nbt_parse(nbt, nlen, 0, &root, NULL),
                MINECPP_NBT_OK);
      const minecpp_nbt_tag_t *level = minecpp_nbt_get(root, "Level");
      ASSERT(level);
      ASSERT_EQ(minecpp_nbt_get(level, "xPos")->v.i32, -1);
      ASSERT_EQ(minecpp_nbt_get(level, "zPos")->v.i32, -12);
      minecpp_nbt_free(root);
      free(nbt);
    }
    free(payload);
  }

  ASSERT_EQ(minecpp_region_read_raw(r, 0, 0, &ver, &payload, &plen),
            MINECPP_REGION_EMPTY);
  ASSERT_EQ(minecpp_region_read_raw(r, 32, 0, &ver, &payload, &plen),
            MINECPP_REGION_BAD_ARG);
  ASSERT_EQ(minecpp_region_read_raw(r, -1, 0, &ver, &payload, &plen),
            MINECPP_REGION_BAD_ARG);

  minecpp_region_close(r);
  remove(tmp);
}

// t2: escrita do zero, overwrite com realocação, reopen.
static void t_write_roundtrip(void) {
  char tmp[256], srccp[256];
  tmp_path(tmp, sizeof tmp, "write");
  tmp_path(srccp, sizeof srccp, "src");
  remove(tmp);

  // Payload A = chunk real (zlib).
  size_t f_len = 0;
  uint8_t *f = read_file(TEST_DATA_DIR "/region/r.-1.-1.mca", &f_len);
  if (!f) return;
  ASSERT_EQ(write_file(srccp, f, f_len), 0);
  free(f);
  minecpp_region_t *src = minecpp_region_open(srccp, 0, NULL);
  ASSERT(src);
  if (!src) {
    remove(srccp);
    return;
  }
  uint8_t aver = 0;
  uint8_t *apay = NULL;
  size_t alen = 0;
  ASSERT_EQ(minecpp_region_read_raw(src, 31, 20, &aver, &apay, &alen),
            MINECPP_REGION_OK);
  minecpp_region_close(src);
  remove(srccp);
  if (!apay) return;

  minecpp_region_t *r = minecpp_region_open(tmp, 1, NULL);
  ASSERT(r);
  if (!r) {
    free(apay);
    return;
  }
  ASSERT_EQ(minecpp_region_chunk_count(r), 0u);

  ASSERT_EQ(minecpp_region_write_raw(r, 0, 0, aver, apay, alen),
            MINECPP_REGION_OK);
  uint8_t ver = 0;
  uint8_t *back = NULL;
  size_t blen = 0;
  ASSERT_EQ(minecpp_region_read_raw(r, 0, 0, &ver, &back, &blen),
            MINECPP_REGION_OK);
  ASSERT_EQ(ver, aver);
  ASSERT_EQ(blen, alen);
  ASSERT(blen > 0 && memcmp(back, apay, alen) == 0);
  free(back);

  uint8_t small[100];
  for (int i = 0; i < 100; i++) small[i] = (uint8_t)(i * 7);
  ASSERT_EQ(minecpp_region_write_raw(r, 31, 31, 2, small, sizeof small),
            MINECPP_REGION_OK);
  ASSERT_EQ(minecpp_region_chunk_count(r), 2u);

  // Overwrite maior (1 setor → 3 setores): força realocação.
  size_t biglen = 9000;
  uint8_t *big = (uint8_t *)malloc(biglen);
  for (size_t i = 0; i < biglen; i++) big[i] = (uint8_t)i;
  ASSERT_EQ(minecpp_region_write_raw(r, 0, 0, 2, big, biglen),
            MINECPP_REGION_OK);
  ASSERT_EQ(minecpp_region_read_raw(r, 0, 0, &ver, &back, &blen),
            MINECPP_REGION_OK);
  ASSERT_EQ(blen, biglen);
  ASSERT(memcmp(back, big, biglen) == 0);
  free(back);
  // Vizinho intacto.
  ASSERT_EQ(minecpp_region_read_raw(r, 31, 31, &ver, &back, &blen),
            MINECPP_REGION_OK);
  ASSERT_EQ(blen, sizeof small);
  ASSERT(memcmp(back, small, sizeof small) == 0);
  free(back);
  free(big);
  free(apay);

  ASSERT(minecpp_region_timestamp(r, 0, 0) != 0);
  minecpp_region_close(r);

  size_t wlen = 0;
  uint8_t *w = read_file(tmp, &wlen);
  ASSERT(w);
  if (w) {
    ASSERT_EQ(wlen % 4096, 0u);
    free(w);
  }

  // Reopen: tudo intacto.
  r = minecpp_region_open(tmp, 0, NULL);
  ASSERT(r);
  if (r) {
    ASSERT_EQ(minecpp_region_chunk_count(r), 2u);
    ASSERT_EQ(minecpp_region_read_raw(r, 0, 0, &ver, &back, &blen),
              MINECPP_REGION_OK);
    ASSERT_EQ(blen, biglen);
    free(back);
    minecpp_region_close(r);
  }
  remove(tmp);
}

// t3: headers/payloads hostis.
static void t_corrupt(void) {
  char tmp[256];
  tmp_path(tmp, sizeof tmp, "corr");
  uint8_t ver = 0;
  uint8_t *out = NULL;
  size_t olen = 0;
  minecpp_region_t *r;

  // (a) offset além do EOF.
  {
    uint8_t hdr[8192] = {0};
    wr32be(hdr, (50u << 8) | 1u);
    ASSERT_EQ(write_file(tmp, hdr, sizeof hdr), 0);
    r = minecpp_region_open(tmp, 0, NULL);
    ASSERT(r);
    ASSERT_EQ(minecpp_region_read_raw(r, 0, 0, &ver, &out, &olen),
              MINECPP_REGION_CORRUPT);
    minecpp_region_close(r);
  }
  // (b) len > count*4096.
  {
    uint8_t buf[8192 + 4096] = {0};
    wr32be(buf, (2u << 8) | 1u);
    wr32be(buf + 8192, 5000);
    ASSERT_EQ(write_file(tmp, buf, sizeof buf), 0);
    r = minecpp_region_open(tmp, 0, NULL);
    ASSERT(r);
    ASSERT_EQ(minecpp_region_read_raw(r, 0, 0, &ver, &out, &olen),
              MINECPP_REGION_CORRUPT);
    minecpp_region_close(r);
  }
  // (c) version 3.
  {
    uint8_t buf[8192 + 4096] = {0};
    wr32be(buf, (2u << 8) | 1u);
    wr32be(buf + 8192, 10);
    buf[8192 + 4] = 3;
    ASSERT_EQ(write_file(tmp, buf, sizeof buf), 0);
    r = minecpp_region_open(tmp, 0, NULL);
    ASSERT(r);
    ASSERT_EQ(minecpp_region_read_raw(r, 0, 0, &ver, &out, &olen),
              MINECPP_REGION_BAD_VERSION);
    minecpp_region_close(r);
  }
  // (d) arquivo truncado: open normaliza com zero-pad (como o vanilla bfv),
  // então a leitura sucede com payload completado por zeros.
  {
    uint8_t buf[8192 + 10] = {0};
    wr32be(buf, (2u << 8) | 1u);
    wr32be(buf + 8192, 100);
    buf[8192 + 4] = 2;
    ASSERT_EQ(write_file(tmp, buf, sizeof buf), 0);
    r = minecpp_region_open(tmp, 0, NULL);
    ASSERT(r);
    ASSERT_EQ(minecpp_region_read_raw(r, 0, 0, &ver, &out, &olen),
              MINECPP_REGION_OK);
    ASSERT_EQ(olen, 99u);
    free(out);
    out = NULL;
    minecpp_region_close(r);
  }
  // (e/f) args inválidos.
  ASSERT(!minecpp_region_open("/tmp/minecpp-nao-existe-xyz.mca", 0, NULL));
  r = minecpp_region_open(tmp, 1, NULL);
  ASSERT(r);
  if (r) {
    uint8_t d[4] = {1, 2, 3, 4};
    ASSERT_EQ(minecpp_region_write_raw(r, 0, 0, 3, d, sizeof d),
              MINECPP_REGION_BAD_ARG);
    ASSERT_EQ(minecpp_region_write_raw(r, 32, 0, 2, d, sizeof d),
              MINECPP_REGION_BAD_ARG);
    minecpp_region_close(r);
  }
  remove(tmp);
}

int main(void) {
  t_open_real();
  t_write_roundtrip();
  t_corrupt();
  if (g_fail == 0) printf("region: todas as verificações passaram\n");
  return g_fail ? 1 : 0;
}
