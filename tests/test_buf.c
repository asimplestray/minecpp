// Testes do codec: golden vectors VarInt + strings + positions + erros.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minecpp/core/buf.h"

static int g_fail = 0;

#define ASSERT(cond)                                         \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      g_fail++;                                              \
    }                                                        \
  } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

// t1: golden VarInt (encode exato + decode).
static void t_varint(void) {
  static const struct {
    int32_t v;
    uint8_t b[5];
    int n;
  } cases[] = {
      {0, {0x00}, 1},
      {1, {0x01}, 1},
      {2, {0x02}, 1},
      {127, {0x7F}, 1},
      {128, {0x80, 0x01}, 2},
      {255, {0xFF, 0x01}, 2},
      {25565, {0xDD, 0xC7, 0x01}, 3},
      {2097151, {0xFF, 0xFF, 0x7F}, 3},
      {2147483647, {0xFF, 0xFF, 0xFF, 0xFF, 0x07}, 5},
      {-1, {0xFF, 0xFF, 0xFF, 0xFF, 0x0F}, 5},
      {-2147483648, {0x80, 0x80, 0x80, 0x80, 0x08}, 5},
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    minecpp_writer_t w = {0};
    minecpp_wr_varint(&w, cases[i].v);
    ASSERT(minecpp_wr_ok(&w));
    size_t n = 0;
    uint8_t *b = minecpp_wr_take(&w, &n);
    ASSERT_EQ(n, (size_t)cases[i].n);
    ASSERT(n == (size_t)cases[i].n && memcmp(b, cases[i].b, n) == 0);
    minecpp_reader_t r = {b, n};
    int32_t v = 0;
    ASSERT_EQ(minecpp_rd_varint(&r, &v), MINECPP_BUF_OK);
    ASSERT_EQ(v, cases[i].v);
    ASSERT_EQ(r.left, 0u);
    free(b);
  }
  // 6 bytes com continuação = overflow (hd joga "VarInt too big").
  uint8_t big[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x01};
  minecpp_reader_t r = {big, sizeof big};
  int32_t v = 0;
  ASSERT_EQ(minecpp_rd_varint(&r, &v), MINECPP_BUF_OVERFLOW);
  // Truncado.
  uint8_t cut[] = {0x80};
  r.p = cut;
  r.left = sizeof cut;
  ASSERT_EQ(minecpp_rd_varint(&r, &v), MINECPP_BUF_TRUNCATED);
  // VarLong: 10 bytes ok, 11 = overflow.
  minecpp_writer_t w = {0};
  minecpp_wr_varlong(&w, -1LL);
  size_t n = 0;
  uint8_t *b = minecpp_wr_take(&w, &n);
  ASSERT_EQ(n, 10u);
  r.p = b;
  r.left = n;
  int64_t lv = 0;
  ASSERT_EQ(minecpp_rd_varlong(&r, &lv), MINECPP_BUF_OK);
  ASSERT_EQ(lv, -1LL);
  free(b);
  uint8_t bigl[11] = {0x80, 0x80, 0x80, 0x80, 0x80,
                      0x80, 0x80, 0x80, 0x80, 0x80, 0x01};
  r.p = bigl;
  r.left = sizeof bigl;
  ASSERT_EQ(minecpp_rd_varlong(&r, &lv), MINECPP_BUF_OVERFLOW);
}

// t2: strings (incl. UTF-8 multibyte) + limites.
static void t_string(void) {
  const char *s = "Ol\xc3\xa1 § flat";
  minecpp_writer_t w = {0};
  minecpp_wr_str(&w, s);
  size_t n = 0;
  uint8_t *b = minecpp_wr_take(&w, &n);
  ASSERT_EQ(b[0], (uint8_t)strlen(s));  // 1 byte de len (< 128)
  minecpp_reader_t r = {b, n};
  char *out = NULL;
  int out_len = 0;
  ASSERT_EQ(minecpp_rd_string(&r, 32767, &out, &out_len), MINECPP_BUF_OK);
  ASSERT_EQ(out_len, (int)strlen(s));
  ASSERT(strcmp(out, s) == 0);
  free(out);
  free(b);
  // Teto: max_chars=4 → 16 bytes; 17 bytes rejeita.
  uint8_t over[] = {17, '0', '1', '2', '3', '4', '5', '6', '7',
                    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f', 'g'};
  r.p = over;
  r.left = sizeof over;
  ASSERT_EQ(minecpp_rd_string(&r, 4, &out, NULL), MINECPP_BUF_BAD_VALUE);
  // Len negativo.
  uint8_t neg[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  r.p = neg;
  r.left = sizeof neg;
  ASSERT_EQ(minecpp_rd_string(&r, 32767, &out, NULL), MINECPP_BUF_BAD_VALUE);
  // Truncada.
  uint8_t cut[] = {5, 'a', 'b'};
  r.p = cut;
  r.left = sizeof cut;
  ASSERT_EQ(minecpp_rd_string(&r, 32767, &out, NULL), MINECPP_BUF_TRUNCATED);
}

// t3: positions (x:26, y:12, z:26 signed).
static void t_pos(void) {
  static const int32_t cases[][3] = {
      {0, 0, 0}, {1, 2, 3}, {-1, -2, -3}, {30000000, 255, 30000000},
      {-30000000, -1, -30000000}, {33554431, 2047, 33554431},
      {-33554432, -2048, -33554432}, {-8, 4, -184},
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    minecpp_writer_t w = {0};
    minecpp_wr_pos(&w, cases[i][0], cases[i][1], cases[i][2]);
    size_t n = 0;
    uint8_t *b = minecpp_wr_take(&w, &n);
    ASSERT_EQ(n, 8u);
    minecpp_reader_t r = {b, n};
    int32_t x = 0, y = 0, z = 0;
    ASSERT_EQ(minecpp_rd_pos(&r, &x, &y, &z), MINECPP_BUF_OK);
    ASSERT_EQ(x, cases[i][0]);
    ASSERT_EQ(y, cases[i][1]);
    ASSERT_EQ(z, cases[i][2]);
    free(b);
  }
  // Golden: (0,0,0) = 8 zeros; (-8,4,-184) confere conta manual.
  minecpp_writer_t w = {0};
  minecpp_wr_pos(&w, -8, 4, -184);
  size_t n = 0;
  uint8_t *b = minecpp_wr_take(&w, &n);
  const uint64_t v = ((uint64_t)0x3FFFFF8ull << 38) | ((uint64_t)4 << 26) |
                     (uint64_t)0x3FFFF48ull;
  for (int i = 0; i < 8; i++)
    ASSERT_EQ(b[i], (uint8_t)(v >> (56 - 8 * i)));
  free(b);
}

// t4: primitivas big-endian.
static void t_prims(void) {
  minecpp_writer_t w = {0};
  minecpp_wr_u16(&w, 0x1234);
  minecpp_wr_i32(&w, -19133);
  minecpp_wr_bool(&w, 7);
  minecpp_wr_f32(&w, 1.5f);
  size_t n = 0;
  uint8_t *b = minecpp_wr_take(&w, &n);
  const uint8_t exp[] = {0x12, 0x34, 0xFF, 0xFF, 0xB5, 0x43, 0x01, 0x3F,
                         0xC0, 0x00, 0x00};
  ASSERT_EQ(n, sizeof exp);
  ASSERT(memcmp(b, exp, sizeof exp) == 0);
  free(b);
}

int main(void) {
  t_varint();
  t_string();
  t_pos();
  t_prims();
  if (g_fail == 0) printf("buf: todas as verificações passaram\n");
  return g_fail ? 1 : 0;
}
