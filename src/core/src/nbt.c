#include "minecpp/core/nbt.h"

#include <stdlib.h>
#include <string.h>

const char *minecpp_nbt_type_name(minecpp_nbt_type_t t) {
  switch (t) {
    case MINECPP_NBT_END: return "End";
    case MINECPP_NBT_BYTE: return "Byte";
    case MINECPP_NBT_SHORT: return "Short";
    case MINECPP_NBT_INT: return "Int";
    case MINECPP_NBT_LONG: return "Long";
    case MINECPP_NBT_FLOAT: return "Float";
    case MINECPP_NBT_DOUBLE: return "Double";
    case MINECPP_NBT_BYTE_ARRAY: return "ByteArray";
    case MINECPP_NBT_STRING: return "String";
    case MINECPP_NBT_LIST: return "List";
    case MINECPP_NBT_COMPOUND: return "Compound";
    case MINECPP_NBT_INT_ARRAY: return "IntArray";
    default: return "?";
  }
}

const char *minecpp_nbt_err_name(minecpp_nbt_err_t e) {
  switch (e) {
    case MINECPP_NBT_OK: return "ok";
    case MINECPP_NBT_TRUNCATED: return "truncated";
    case MINECPP_NBT_BAD_ID: return "bad-id";
    case MINECPP_NBT_BAD_DATA: return "bad-data";
    case MINECPP_NBT_TOO_DEEP: return "too-deep";
    case MINECPP_NBT_OVER_BUDGET: return "over-budget";
    case MINECPP_NBT_NOMEM: return "nomem";
    default: return "?";
  }
}

// ---------------------------------------------------------------- reader

typedef struct {
  const uint8_t *p;
  size_t left;
  size_t budget;  // bytes que ainda pode consumir/alocar
  unsigned depth;
} reader_t;

static minecpp_nbt_err_t charge(reader_t *r, size_t n) {
  if (n > r->left) return MINECPP_NBT_TRUNCATED;
  if (n > r->budget) return MINECPP_NBT_OVER_BUDGET;
  r->budget -= n;
  return MINECPP_NBT_OK;
}

static minecpp_nbt_err_t take(reader_t *r, void *dst, size_t n) {
  minecpp_nbt_err_t e = charge(r, n);
  if (e != MINECPP_NBT_OK) return e;
  memcpy(dst, r->p, n);
  r->p += n;
  r->left -= n;
  return MINECPP_NBT_OK;
}

static minecpp_nbt_err_t take_u8(reader_t *r, uint8_t *v) { return take(r, v, 1); }

static minecpp_nbt_err_t take_u16(reader_t *r, uint16_t *v) {
  uint8_t b[2];
  minecpp_nbt_err_t e = take(r, b, 2);
  if (e != MINECPP_NBT_OK) return e;
  *v = (uint16_t)((uint16_t)b[0] << 8 | b[1]);
  return MINECPP_NBT_OK;
}

static minecpp_nbt_err_t take_u32(reader_t *r, uint32_t *v) {
  uint8_t b[4];
  minecpp_nbt_err_t e = take(r, b, 4);
  if (e != MINECPP_NBT_OK) return e;
  *v = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
  return MINECPP_NBT_OK;
}

static minecpp_nbt_err_t take_i32(reader_t *r, int32_t *v) {
  uint32_t u;
  minecpp_nbt_err_t e = take_u32(r, &u);
  if (e == MINECPP_NBT_OK) *v = (int32_t)u;
  return e;
}

// nome writeUTF: u16 len + bytes crus. Guarda bytes + NUL de conveniência.
static minecpp_nbt_err_t take_name(reader_t *r, char **out, uint16_t *out_len) {
  uint16_t n = 0;
  minecpp_nbt_err_t e = take_u16(r, &n);
  if (e != MINECPP_NBT_OK) return e;
  // +1 do NUL também sai do budget (anti-OOM).
  if ((size_t)n + 1 > r->budget) return MINECPP_NBT_OVER_BUDGET;
  if ((size_t)n > r->left) return MINECPP_NBT_TRUNCATED;
  char *s = (char *)malloc((size_t)n + 1);
  if (!s) return MINECPP_NBT_NOMEM;
  memcpy(s, r->p, n);
  s[n] = '\0';
  r->p += n;
  r->left -= n;
  r->budget -= (size_t)n + 1;
  *out = s;
  if (out_len) *out_len = n;
  return MINECPP_NBT_OK;
}

static void tag_free_children(minecpp_nbt_tag_t *t);

static minecpp_nbt_err_t parse_payload(reader_t *r, minecpp_nbt_type_t type,
                                       minecpp_nbt_tag_t *t) {
  minecpp_nbt_err_t e;
  uint8_t b8;
  uint16_t u16;
  uint32_t u32;
  int32_t i32;
  uint32_t count;

  if (r->depth > MINECPP_NBT_MAX_DEPTH) return MINECPP_NBT_TOO_DEEP;

  switch (type) {
    case MINECPP_NBT_END:
      return MINECPP_NBT_BAD_DATA;  // END nunca tem payload
    case MINECPP_NBT_BYTE:
      if ((e = take_u8(r, &b8)) != MINECPP_NBT_OK) return e;
      t->v.i8 = (int8_t)b8;
      return MINECPP_NBT_OK;
    case MINECPP_NBT_SHORT: {
      uint8_t b[2];
      if ((e = take(r, b, 2)) != MINECPP_NBT_OK) return e;
      t->v.i16 = (int16_t)((uint16_t)b[0] << 8 | b[1]);
      return MINECPP_NBT_OK;
    }
    case MINECPP_NBT_INT:
      return take_i32(r, &t->v.i32);
    case MINECPP_NBT_LONG: {
      uint8_t b[8];
      if ((e = take(r, b, 8)) != MINECPP_NBT_OK) return e;
      uint64_t u = 0;
      for (int i = 0; i < 8; i++) u = u << 8 | b[i];
      t->v.i64 = (int64_t)u;
      return MINECPP_NBT_OK;
    }
    case MINECPP_NBT_FLOAT: {
      uint32_t u;
      if ((e = take_u32(r, &u)) != MINECPP_NBT_OK) return e;
      memcpy(&t->v.f32, &u, 4);
      return MINECPP_NBT_OK;
    }
    case MINECPP_NBT_DOUBLE: {
      uint8_t b[8];
      if ((e = take(r, b, 8)) != MINECPP_NBT_OK) return e;
      uint64_t u = 0;
      for (int i = 0; i < 8; i++) u = u << 8 | b[i];
      memcpy(&t->v.f64, &u, 8);
      return MINECPP_NBT_OK;
    }
    case MINECPP_NBT_BYTE_ARRAY:
      if ((e = take_i32(r, &i32)) != MINECPP_NBT_OK) return e;
      if (i32 < 0) return MINECPP_NBT_BAD_DATA;
      if ((uint32_t)i32 > r->budget || (uint32_t)i32 > r->left) {
        // Distingue truncado de estouro: cabe no input mas não no budget?
        return (uint32_t)i32 > r->left ? MINECPP_NBT_TRUNCATED
                                       : MINECPP_NBT_OVER_BUDGET;
      }
      t->v.bytes.data = (uint8_t *)malloc((size_t)i32 ? (size_t)i32 : 1);
      if (!t->v.bytes.data) return MINECPP_NBT_NOMEM;
      t->v.bytes.len = (size_t)i32;
      if (i32) {
        memcpy(t->v.bytes.data, r->p, (size_t)i32);
        r->p += (size_t)i32;
        r->left -= (size_t)i32;
        r->budget -= (size_t)i32;
      }
      return MINECPP_NBT_OK;
    case MINECPP_NBT_STRING:
      if ((e = take_u16(r, &u16)) != MINECPP_NBT_OK) return e;
      if ((size_t)u16 + 1 > r->budget) return MINECPP_NBT_OVER_BUDGET;
      if ((size_t)u16 > r->left) return MINECPP_NBT_TRUNCATED;
      t->v.text.data = (uint8_t *)malloc((size_t)u16 + 1);
      if (!t->v.text.data) return MINECPP_NBT_NOMEM;
      memcpy(t->v.text.data, r->p, u16);
      t->v.text.data[u16] = '\0';
      t->v.text.len = u16;
      r->p += u16;
      r->left -= u16;
      r->budget -= (size_t)u16 + 1;
      return MINECPP_NBT_OK;
    case MINECPP_NBT_INT_ARRAY:
      if ((e = take_i32(r, &i32)) != MINECPP_NBT_OK) return e;
      if (i32 < 0) return MINECPP_NBT_BAD_DATA;
      if (i32 > 0 && (uint32_t)i32 > r->budget / 4) return MINECPP_NBT_OVER_BUDGET;
      if (i32 > 0 && (size_t)i32 > r->left / 4) return MINECPP_NBT_TRUNCATED;
      t->v.ints.data = (int32_t *)malloc((size_t)(i32 ? i32 : 1) * 4);
      if (!t->v.ints.data) return MINECPP_NBT_NOMEM;
      t->v.ints.len = (size_t)i32;
      for (int32_t i = 0; i < i32; i++) {
        if ((e = take_u32(r, &u32)) != MINECPP_NBT_OK) {
          t->v.ints.len = (size_t)i;  // free parcial consistente
          return e;
        }
        t->v.ints.data[i] = (int32_t)u32;
      }
      return MINECPP_NBT_OK;
    case MINECPP_NBT_LIST: {
      uint8_t id;
      if ((e = take_u8(r, &id)) != MINECPP_NBT_OK) return e;
      if (id > MINECPP_NBT_INT_ARRAY) return MINECPP_NBT_BAD_ID;
      if ((e = take_i32(r, &i32)) != MINECPP_NBT_OK) return e;
      if (i32 < 0) return MINECPP_NBT_BAD_DATA;
      count = (uint32_t)i32;
      t->v.list.elem = (minecpp_nbt_type_t)id;
      t->v.list.items = NULL;
      t->v.list.len = 0;
      if (id == MINECPP_NBT_END) {
        return count == 0 ? MINECPP_NBT_OK : MINECPP_NBT_BAD_DATA;
      }
      if (count == 0) return MINECPP_NBT_OK;
      // Teto anti-OOM: ponteiros também saem do budget.
      if (count > r->budget / sizeof(void *)) return MINECPP_NBT_OVER_BUDGET;
      t->v.list.items =
          (minecpp_nbt_tag_t **)calloc(count, sizeof(minecpp_nbt_tag_t *));
      if (!t->v.list.items) return MINECPP_NBT_NOMEM;
      r->budget -= count * sizeof(void *);
      r->depth++;
      for (uint32_t i = 0; i < count; i++) {
        minecpp_nbt_tag_t *c =
            (minecpp_nbt_tag_t *)calloc(1, sizeof(minecpp_nbt_tag_t));
        if (!c) {
          t->v.list.len = i;
          r->depth--;
          return MINECPP_NBT_NOMEM;
        }
        if (sizeof(minecpp_nbt_tag_t) > r->budget) {
          free(c);
          t->v.list.len = i;
          r->depth--;
          return MINECPP_NBT_OVER_BUDGET;
        }
        r->budget -= sizeof(minecpp_nbt_tag_t);
        c->type = (minecpp_nbt_type_t)id;
        e = parse_payload(r, (minecpp_nbt_type_t)id, c);
        if (e != MINECPP_NBT_OK) {
          minecpp_nbt_free(c);
          t->v.list.len = i;
          r->depth--;
          return e;
        }
        t->v.list.items[i] = c;
      }
      t->v.list.len = count;
      r->depth--;
      return MINECPP_NBT_OK;
    }
    case MINECPP_NBT_COMPOUND: {
      minecpp_nbt_tag_t **items = NULL;
      size_t len = 0, cap = 0;
      r->depth++;
      for (;;) {
        uint8_t id;
        if ((e = take_u8(r, &id)) != MINECPP_NBT_OK) {
          // Libera o que já leu (sem tocar em t, que o caller libera).
          for (size_t i = 0; i < len; i++) minecpp_nbt_free(items[i]);
          free(items);
          r->depth--;
          return e;
        }
        if (id == MINECPP_NBT_END) break;
        if (id > MINECPP_NBT_INT_ARRAY) {
          for (size_t i = 0; i < len; i++) minecpp_nbt_free(items[i]);
          free(items);
          r->depth--;
          return MINECPP_NBT_BAD_ID;
        }
        if (len == cap) {
          size_t ncap = cap ? cap * 2 : 8;
          if (ncap * sizeof(void *) > r->budget) {
            for (size_t i = 0; i < len; i++) minecpp_nbt_free(items[i]);
            free(items);
            r->depth--;
            return MINECPP_NBT_OVER_BUDGET;
          }
          minecpp_nbt_tag_t **ni =
              (minecpp_nbt_tag_t **)realloc(items, ncap * sizeof(void *));
          if (!ni) {
            for (size_t i = 0; i < len; i++) minecpp_nbt_free(items[i]);
            free(items);
            r->depth--;
            return MINECPP_NBT_NOMEM;
          }
          items = ni;
          r->budget -= (ncap - cap) * sizeof(void *);
          cap = ncap;
        }
        minecpp_nbt_tag_t *c =
            (minecpp_nbt_tag_t *)calloc(1, sizeof(minecpp_nbt_tag_t));
        if (!c) {
          for (size_t i = 0; i < len; i++) minecpp_nbt_free(items[i]);
          free(items);
          r->depth--;
          return MINECPP_NBT_NOMEM;
        }
        if (sizeof(minecpp_nbt_tag_t) > r->budget) {
          free(c);
          for (size_t i = 0; i < len; i++) minecpp_nbt_free(items[i]);
          free(items);
          r->depth--;
          return MINECPP_NBT_OVER_BUDGET;
        }
        r->budget -= sizeof(minecpp_nbt_tag_t);
        c->type = (minecpp_nbt_type_t)id;
        if ((e = take_name(r, &c->name, &c->name_len)) != MINECPP_NBT_OK) {
          free(c);
          for (size_t i = 0; i < len; i++) minecpp_nbt_free(items[i]);
          free(items);
          r->depth--;
          return e;
        }
        if ((e = parse_payload(r, (minecpp_nbt_type_t)id, c)) !=
            MINECPP_NBT_OK) {
          minecpp_nbt_free(c);
          for (size_t i = 0; i < len; i++) minecpp_nbt_free(items[i]);
          free(items);
          r->depth--;
          return e;
        }
        items[len++] = c;
      }
      r->depth--;
      t->v.compound.items = items;
      t->v.compound.len = len;
      return MINECPP_NBT_OK;
    }
  }
  return MINECPP_NBT_BAD_ID;  // inalcançável
}

minecpp_nbt_err_t minecpp_nbt_parse(const uint8_t *data, size_t len,
                                    size_t max_bytes,
                                    minecpp_nbt_tag_t **out,
                                    size_t *consumed) {
  if (!data || !out) return MINECPP_NBT_BAD_DATA;
  reader_t r = {data, len,
                max_bytes ? max_bytes : MINECPP_NBT_DEFAULT_BUDGET, 0};
  uint8_t id = 0;
  minecpp_nbt_err_t e = take_u8(&r, &id);
  if (e != MINECPP_NBT_OK) return e;
  if (id == MINECPP_NBT_END || id > MINECPP_NBT_INT_ARRAY)
    return MINECPP_NBT_BAD_ID;
  if (sizeof(minecpp_nbt_tag_t) > r.budget) return MINECPP_NBT_OVER_BUDGET;
  minecpp_nbt_tag_t *t =
      (minecpp_nbt_tag_t *)calloc(1, sizeof(minecpp_nbt_tag_t));
  if (!t) return MINECPP_NBT_NOMEM;
  r.budget -= sizeof(minecpp_nbt_tag_t);
  t->type = (minecpp_nbt_type_t)id;
  if ((e = take_name(&r, &t->name, &t->name_len)) != MINECPP_NBT_OK) {
    free(t);
    return e;
  }
  if ((e = parse_payload(&r, t->type, t)) != MINECPP_NBT_OK) {
    minecpp_nbt_free(t);
    return e;
  }
  *out = t;
  if (consumed) *consumed = len - r.left;
  return MINECPP_NBT_OK;
}

// ---------------------------------------------------------------- writer

typedef struct {
  uint8_t *buf;
  size_t len, cap;
  int oom;
} writer_t;

static void w_need(writer_t *w, size_t n) {
  if (w->oom) return;
  if (w->len + n <= w->cap) return;
  size_t ncap = w->cap ? w->cap : 256;
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

static void w_raw(writer_t *w, const void *d, size_t n) {
  w_need(w, n);
  if (w->oom) return;
  memcpy(w->buf + w->len, d, n);
  w->len += n;
}

static void w_u8(writer_t *w, uint8_t v) { w_raw(w, &v, 1); }

static void w_u16(writer_t *w, uint16_t v) {
  uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v};
  w_raw(w, b, 2);
}

static void w_u32(writer_t *w, uint32_t v) {
  uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8),
                  (uint8_t)v};
  w_raw(w, b, 4);
}

static void w_name(writer_t *w, const char *name, uint16_t name_len) {
  w_u16(w, name_len);
  if (name_len) w_raw(w, name, name_len);
}

static void w_payload(writer_t *w, const minecpp_nbt_tag_t *t);

static void w_named(writer_t *w, const minecpp_nbt_tag_t *t) {
  w_u8(w, (uint8_t)t->type);
  w_name(w, t->name ? t->name : "", t->name ? t->name_len : 0);
  w_payload(w, t);
}

static void w_payload(writer_t *w, const minecpp_nbt_tag_t *t) {
  switch (t->type) {
    case MINECPP_NBT_END:
      return;
    case MINECPP_NBT_BYTE:
      w_u8(w, (uint8_t)t->v.i8);
      return;
    case MINECPP_NBT_SHORT:
      w_u16(w, (uint16_t)t->v.i16);
      return;
    case MINECPP_NBT_INT:
      w_u32(w, (uint32_t)t->v.i32);
      return;
    case MINECPP_NBT_LONG: {
      uint64_t u = (uint64_t)t->v.i64;
      uint8_t b[8];
      for (int i = 7; i >= 0; i--) {
        b[i] = (uint8_t)u;
        u >>= 8;
      }
      w_raw(w, b, 8);
      return;
    }
    case MINECPP_NBT_FLOAT: {
      uint32_t u;
      memcpy(&u, &t->v.f32, 4);
      w_u32(w, u);
      return;
    }
    case MINECPP_NBT_DOUBLE: {
      uint64_t u;
      memcpy(&u, &t->v.f64, 8);
      uint8_t b[8];
      for (int i = 7; i >= 0; i--) {
        b[i] = (uint8_t)u;
        u >>= 8;
      }
      w_raw(w, b, 8);
      return;
    }
    case MINECPP_NBT_BYTE_ARRAY:
      w_u32(w, (uint32_t)t->v.bytes.len);
      if (t->v.bytes.len) w_raw(w, t->v.bytes.data, t->v.bytes.len);
      return;
    case MINECPP_NBT_STRING:
      w_u16(w, t->v.text.len);
      if (t->v.text.len) w_raw(w, t->v.text.data, t->v.text.len);
      return;
    case MINECPP_NBT_INT_ARRAY:
      w_u32(w, (uint32_t)t->v.ints.len);
      for (size_t i = 0; i < t->v.ints.len; i++)
        w_u32(w, (uint32_t)t->v.ints.data[i]);
      return;
    case MINECPP_NBT_LIST:
      w_u8(w, (uint8_t)(t->v.list.len ? t->v.list.elem : MINECPP_NBT_END));
      w_u32(w, (uint32_t)t->v.list.len);
      for (size_t i = 0; i < t->v.list.len; i++)
        w_payload(w, t->v.list.items[i]);  // sem nome (vanilla fv)
      return;
    case MINECPP_NBT_COMPOUND:
      for (size_t i = 0; i < t->v.compound.len; i++)
        w_named(w, t->v.compound.items[i]);
      w_u8(w, 0);  // TAG_End
      return;
  }
}

minecpp_nbt_err_t minecpp_nbt_serialize(const minecpp_nbt_tag_t *root,
                                        uint8_t **out, size_t *out_len) {
  if (!root || !out || !out_len) return MINECPP_NBT_BAD_DATA;
  if (root->type == MINECPP_NBT_END) return MINECPP_NBT_BAD_DATA;
  writer_t w = {NULL, 0, 0, 0};
  w_named(&w, root);
  if (w.oom) {
    free(w.buf);
    return MINECPP_NBT_NOMEM;
  }
  *out = w.buf;
  *out_len = w.len;
  return MINECPP_NBT_OK;
}

// ---------------------------------------------------------------- free/new

static void tag_free_children(minecpp_nbt_tag_t *t) {
  size_t i;
  switch (t->type) {
    case MINECPP_NBT_BYTE_ARRAY:
      free(t->v.bytes.data);
      break;
    case MINECPP_NBT_STRING:
      free(t->v.text.data);
      break;
    case MINECPP_NBT_INT_ARRAY:
      free(t->v.ints.data);
      break;
    case MINECPP_NBT_LIST:
      for (i = 0; i < t->v.list.len; i++) minecpp_nbt_free(t->v.list.items[i]);
      free(t->v.list.items);
      break;
    case MINECPP_NBT_COMPOUND:
      for (i = 0; i < t->v.compound.len; i++)
        minecpp_nbt_free(t->v.compound.items[i]);
      free(t->v.compound.items);
      break;
    default:
      break;
  }
}

void minecpp_nbt_free(minecpp_nbt_tag_t *tag) {
  if (!tag) return;
  free(tag->name);
  tag_free_children(tag);
  free(tag);
}

minecpp_nbt_tag_t *minecpp_nbt_new(minecpp_nbt_type_t type, const char *name) {
  if (type > MINECPP_NBT_INT_ARRAY) return NULL;
  minecpp_nbt_tag_t *t =
      (minecpp_nbt_tag_t *)calloc(1, sizeof(minecpp_nbt_tag_t));
  if (!t) return NULL;
  t->type = type;
  if (name) {
    size_t n = strlen(name);
    if (n > 0xFFFF) {
      free(t);
      return NULL;
    }
    t->name = (char *)malloc(n + 1);
    if (!t->name) {
      free(t);
      return NULL;
    }
    memcpy(t->name, name, n + 1);
    t->name_len = (uint16_t)n;
  }
  return t;
}

int minecpp_nbt_add(minecpp_nbt_tag_t *parent, minecpp_nbt_tag_t *child) {
  if (!parent || !child) return -1;
  if (parent->type == MINECPP_NBT_COMPOUND) {
    if (!child->name) return -1;
    size_t n = parent->v.compound.len + 1;
    minecpp_nbt_tag_t **ni = (minecpp_nbt_tag_t **)realloc(
        parent->v.compound.items, n * sizeof(void *));
    if (!ni) return -1;
    parent->v.compound.items = ni;
    parent->v.compound.items[parent->v.compound.len++] = child;
    return 0;
  }
  if (parent->type == MINECPP_NBT_LIST) {
    if (child->name) return -1;
    if (parent->v.list.len == 0) {
      if (child->type == MINECPP_NBT_END) return -1;
      parent->v.list.elem = child->type;
    } else if (child->type != parent->v.list.elem) {
      return -1;
    }
    size_t n = parent->v.list.len + 1;
    minecpp_nbt_tag_t **ni = (minecpp_nbt_tag_t **)realloc(
        parent->v.list.items, n * sizeof(void *));
    if (!ni) return -1;
    parent->v.list.items = ni;
    parent->v.list.items[parent->v.list.len++] = child;
    return 0;
  }
  return -1;
}

const minecpp_nbt_tag_t *minecpp_nbt_get(const minecpp_nbt_tag_t *compound,
                                         const char *name) {
  size_t i;
  if (!compound || !name || compound->type != MINECPP_NBT_COMPOUND)
    return NULL;
  for (i = 0; i < compound->v.compound.len; i++) {
    const minecpp_nbt_tag_t *c = compound->v.compound.items[i];
    if (c->name && strcmp(c->name, name) == 0) return c;
  }
  return NULL;
}

minecpp_nbt_tag_t *minecpp_nbt_detach(minecpp_nbt_tag_t *compound,
                                      const char *name) {
  size_t i;
  if (!compound || !name || compound->type != MINECPP_NBT_COMPOUND)
    return NULL;
  for (i = 0; i < compound->v.compound.len; i++) {
    minecpp_nbt_tag_t *c = compound->v.compound.items[i];
    if (c->name && strcmp(c->name, name) == 0) {
      size_t tail = compound->v.compound.len - i - 1;
      if (tail)
        memmove(&compound->v.compound.items[i],
                &compound->v.compound.items[i + 1],
                tail * sizeof(void *));
      compound->v.compound.len--;
      return c;
    }
  }
  return NULL;
}

int minecpp_nbt_equal(const minecpp_nbt_tag_t *a, const minecpp_nbt_tag_t *b) {
  size_t i;
  if (a == b) return 1;
  if (!a || !b) return 0;
  if (a->type != b->type) return 0;
  // Nomes: elementos de lista têm NULL nos dois; em compounds a ordem é
  // ignorada mas os nomes dos pares devem bater (checado abaixo).
  switch (a->type) {
    case MINECPP_NBT_END:
      return 1;
    case MINECPP_NBT_BYTE:
      return a->v.i8 == b->v.i8;
    case MINECPP_NBT_SHORT:
      return a->v.i16 == b->v.i16;
    case MINECPP_NBT_INT:
      return a->v.i32 == b->v.i32;
    case MINECPP_NBT_LONG:
      return a->v.i64 == b->v.i64;
    case MINECPP_NBT_FLOAT:
      return memcmp(&a->v.f32, &b->v.f32, 4) == 0;
    case MINECPP_NBT_DOUBLE:
      return memcmp(&a->v.f64, &b->v.f64, 8) == 0;
    case MINECPP_NBT_BYTE_ARRAY:
      return a->v.bytes.len == b->v.bytes.len &&
             (a->v.bytes.len == 0 ||
              memcmp(a->v.bytes.data, b->v.bytes.data, a->v.bytes.len) == 0);
    case MINECPP_NBT_STRING:
      return a->v.text.len == b->v.text.len &&
             (a->v.text.len == 0 ||
              memcmp(a->v.text.data, b->v.text.data, a->v.text.len) == 0);
    case MINECPP_NBT_INT_ARRAY:
      return a->v.ints.len == b->v.ints.len &&
             (a->v.ints.len == 0 ||
              memcmp(a->v.ints.data, b->v.ints.data,
                     a->v.ints.len * 4) == 0);
    case MINECPP_NBT_LIST:
      if (a->v.list.elem != b->v.list.elem ||
          a->v.list.len != b->v.list.len)
        return 0;
      for (i = 0; i < a->v.list.len; i++)
        if (!minecpp_nbt_equal(a->v.list.items[i], b->v.list.items[i]))
          return 0;
      return 1;
    case MINECPP_NBT_COMPOUND: {
      if (a->v.compound.len != b->v.compound.len) return 0;
      for (i = 0; i < a->v.compound.len; i++) {
        const minecpp_nbt_tag_t *ca = a->v.compound.items[i];
        const minecpp_nbt_tag_t *cb =
            ca->name ? minecpp_nbt_get(b, ca->name) : NULL;
        if (!cb || !minecpp_nbt_equal(ca, cb)) return 0;
      }
      return 1;
    }
  }
  return 0;
}

minecpp_nbt_tag_t *minecpp_nbt_clone(const minecpp_nbt_tag_t *s) {
  minecpp_nbt_tag_t *c;
  size_t i;
  if (!s) return NULL;
  c = (minecpp_nbt_tag_t *)calloc(1, sizeof *c);
  if (!c) return NULL;
  c->type = s->type;
  if (s->name) {
    c->name = (char *)malloc(s->name_len + 1);
    if (!c->name) {
      free(c);
      return NULL;
    }
    memcpy(c->name, s->name, s->name_len + 1);
    c->name_len = s->name_len;
  }
  switch (s->type) {
    case MINECPP_NBT_END:
      break;
    case MINECPP_NBT_BYTE:
      c->v.i8 = s->v.i8;
      break;
    case MINECPP_NBT_SHORT:
      c->v.i16 = s->v.i16;
      break;
    case MINECPP_NBT_INT:
      c->v.i32 = s->v.i32;
      break;
    case MINECPP_NBT_LONG:
      c->v.i64 = s->v.i64;
      break;
    case MINECPP_NBT_FLOAT:
      c->v.f32 = s->v.f32;
      break;
    case MINECPP_NBT_DOUBLE:
      c->v.f64 = s->v.f64;
      break;
    case MINECPP_NBT_BYTE_ARRAY:
      if (s->v.bytes.len) {
        c->v.bytes.data = (uint8_t *)malloc(s->v.bytes.len);
        if (!c->v.bytes.data) goto oom;
        memcpy(c->v.bytes.data, s->v.bytes.data, s->v.bytes.len);
      }
      c->v.bytes.len = s->v.bytes.len;
      break;
    case MINECPP_NBT_STRING:
      c->v.text.data = (uint8_t *)malloc(s->v.text.len + 1);
      if (!c->v.text.data) goto oom;
      memcpy(c->v.text.data, s->v.text.data, s->v.text.len + 1);
      c->v.text.len = s->v.text.len;
      break;
    case MINECPP_NBT_INT_ARRAY:
      if (s->v.ints.len) {
        c->v.ints.data = (int32_t *)malloc(s->v.ints.len * 4);
        if (!c->v.ints.data) goto oom;
        memcpy(c->v.ints.data, s->v.ints.data, s->v.ints.len * 4);
      }
      c->v.ints.len = s->v.ints.len;
      break;
    case MINECPP_NBT_LIST:
      c->v.list.elem = s->v.list.elem;
      for (i = 0; i < s->v.list.len; i++) {
        minecpp_nbt_tag_t *k = minecpp_nbt_clone(s->v.list.items[i]);
        if (!k || minecpp_nbt_add(c, k) != 0) {
          minecpp_nbt_free(k);
          goto oom;
        }
      }
      break;
    case MINECPP_NBT_COMPOUND:
      for (i = 0; i < s->v.compound.len; i++) {
        minecpp_nbt_tag_t *k = minecpp_nbt_clone(s->v.compound.items[i]);
        if (!k || minecpp_nbt_add(c, k) != 0) {
          minecpp_nbt_free(k);
          goto oom;
        }
      }
      break;
  }
  return c;
oom:
  minecpp_nbt_free(c);
  return NULL;
}
