/*  crapto1.c
    Copyright (C) 2008-2014 bla <blapost@gmail.com>
    License: GPLv2+

    Modified for ESP32: uses PSRAM for large allocations, LOWMEM mode,
    reduced table sizes to fit 8MB PSRAM.
*/
#define LOWMEM  /* skip 1MB filter LUT — not feasible on ESP32 */

#include "crapto1.h"
#include <stdlib.h>

#ifdef ESP32
#include <esp_heap_caps.h>
/* Try PSRAM first, fall back to regular heap */
static void* big_malloc(size_t size)
{
  void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = malloc(size);
  return p;
}
static void big_free(void *p) { free(p); }
#else
#define big_malloc malloc
#define big_free   free
#endif

/* Table size limits — reduced for ESP32 (8MB PSRAM)
 * Original: << 21 (8MB each), << 18 (2MB statelist)
 * ESP32:    << 19 (2MB each), << 17 (1MB statelist)
 * Total: ~5MB vs original ~18MB
 */
#ifdef ESP32
#define TBL_SHIFT  19
#define SL_SHIFT   17
#else
#define TBL_SHIFT  21
#define SL_SHIFT   18
#endif

static void quicksort(uint32_t* const start, uint32_t* const stop)
{
  uint32_t *it = start + 1, *rit = stop, t;
  if(it > rit) return;
  while(it < rit)
    if(*it <= *start) ++it;
    else if(*rit > *start) --rit;
    else t = *it, *it = *rit, *rit = t;
  if(*rit >= *start) --rit;
  if(rit != start) t = *rit, *rit = *start, *start = t;
  quicksort(start, rit - 1);
  quicksort(rit + 1, stop);
}

static inline uint32_t* binsearch(uint32_t *start, uint32_t *stop)
{
  uint32_t mid, val = *stop & 0xff000000;
  while(start != stop)
    if(start[mid = (stop - start) >> 1] > val)
      stop = &start[mid];
    else
      start += mid + 1;
  return start;
}

static inline void
update_contribution(uint32_t *item, const uint32_t mask1, const uint32_t mask2)
{
  uint32_t p = *item >> 25;
  p = p << 1 | parity(*item & mask1);
  p = p << 1 | parity(*item & mask2);
  *item = p << 24 | (*item & 0xffffff);
}

static inline void
extend_table(uint32_t *tbl, uint32_t **end, int bit, int m1, int m2, uint32_t in,
             uint32_t *tbl_start, uint32_t tbl_max)
{
  in <<= 24;
  for(*tbl <<= 1; tbl <= *end; *++tbl <<= 1)
    if(filter(*tbl) ^ filter(*tbl | 1)) {
      *tbl |= filter(*tbl) ^ bit;
      update_contribution(tbl, m1, m2);
      *tbl ^= in;
    } else if(filter(*tbl) == bit) {
      if((uint32_t)(*end - tbl_start) >= tbl_max - 1) {
        *tbl-- = *(*end)--;
        continue;
      }
      *++*end = tbl[1];
      tbl[1] = tbl[0] | 1;
      update_contribution(tbl, m1, m2);
      *tbl++ ^= in;
      update_contribution(tbl, m1, m2);
      *tbl ^= in;
    } else
      *tbl-- = *(*end)--;
}

static inline void extend_table_simple(uint32_t *tbl, uint32_t **end, int bit,
                                       uint32_t *tbl_start, uint32_t tbl_max)
{
  for(*tbl <<= 1; tbl <= *end; *++tbl <<= 1)
    if(filter(*tbl) ^ filter(*tbl | 1))
      *tbl |= filter(*tbl) ^ bit;
    else if(filter(*tbl) == bit) {
      if((uint32_t)(*end - tbl_start) >= tbl_max - 1) {
        *tbl-- = *(*end)--;
        continue;
      }
      *++*end = *++tbl;
      *tbl = tbl[-1] | 1;
    } else
      *tbl-- = *(*end)--;
}

static struct Crypto1State*
recover(uint32_t *o_head, uint32_t *o_tail, uint32_t oks,
  uint32_t *e_head, uint32_t *e_tail, uint32_t eks, int rem,
  struct Crypto1State *sl, uint32_t in,
  uint32_t *o_start, uint32_t o_max, uint32_t *e_start, uint32_t e_max)
{
  uint32_t *o, *e, i;
  if(rem == -1) {
    for(e = e_head; e <= e_tail; ++e) {
      *e = *e << 1 ^ parity(*e & LF_POLY_EVEN) ^ !!(in & 4);
      for(o = o_head; o <= o_tail; ++o, ++sl) {
        sl->even = *o;
        sl->odd = *e ^ parity(*o & LF_POLY_ODD);
        sl[1].odd = sl[1].even = 0;
      }
    }
    return sl;
  }
  for(i = 0; i < 4 && rem--; i++) {
    oks >>= 1; eks >>= 1; in >>= 2;
    extend_table(o_head, &o_tail, oks & 1,
           LF_POLY_EVEN << 1 | 1, LF_POLY_ODD << 1, 0,
           o_start, o_max);
    if(o_head > o_tail) return sl;
    extend_table(e_head, &e_tail, eks & 1,
           LF_POLY_ODD, LF_POLY_EVEN << 1 | 1, in & 3,
           e_start, e_max);
    if(e_head > e_tail) return sl;
  }
  quicksort(o_head, o_tail);
  quicksort(e_head, e_tail);
  while(o_tail >= o_head && e_tail >= e_head)
    if(((*o_tail ^ *e_tail) >> 24) == 0) {
      o_tail = binsearch(o_head, o = o_tail);
      e_tail = binsearch(e_head, e = e_tail);
      sl = recover(o_tail--, o, oks,
             e_tail--, e, eks, rem, sl, in,
             o_start, o_max, e_start, e_max);
    }
    else if(*o_tail > *e_tail)
      o_tail = binsearch(o_head, o_tail) - 1;
    else
      e_tail = binsearch(e_head, e_tail) - 1;
  return sl;
}

struct Crypto1State* lfsr_recovery32(uint32_t ks2, uint32_t in)
{
  struct Crypto1State *statelist;
  uint32_t *odd_head = 0, *odd_tail = 0, oks = 0;
  uint32_t *even_head = 0, *even_tail = 0, eks = 0;
  int i;
  uint32_t tbl_size = (uint32_t)1 << TBL_SHIFT;

  for(i = 31; i >= 0; i -= 2)
    oks = oks << 1 | BEBIT(ks2, i);
  for(i = 30; i >= 0; i -= 2)
    eks = eks << 1 | BEBIT(ks2, i);

  odd_head = odd_tail = (uint32_t*)big_malloc(sizeof(uint32_t) << TBL_SHIFT);
  even_head = even_tail = (uint32_t*)big_malloc(sizeof(uint32_t) << TBL_SHIFT);
  statelist = (struct Crypto1State*)big_malloc(sizeof(struct Crypto1State) << SL_SHIFT);
  if(!odd_tail-- || !even_tail-- || !statelist) {
    big_free(statelist);
    statelist = 0;
    goto out;
  }

  statelist->odd = statelist->even = 0;

  for(i = 1 << 20; i >= 0; --i) {
    if(filter(i) == (oks & 1)) {
      if((uint32_t)(odd_tail - odd_head) < tbl_size - 1)
        *++odd_tail = i;
    }
    if(filter(i) == (eks & 1)) {
      if((uint32_t)(even_tail - even_head) < tbl_size - 1)
        *++even_tail = i;
    }
  }

  for(i = 0; i < 4; i++) {
    extend_table_simple(odd_head,  &odd_tail, (oks >>= 1) & 1, odd_head, tbl_size);
    extend_table_simple(even_head, &even_tail, (eks >>= 1) & 1, even_head, tbl_size);
  }

  in = (in >> 16 & 0xff) | (in << 16) | (in & 0xff00);
  recover(odd_head, odd_tail, oks,
    even_head, even_tail, eks, 11, statelist, in << 1,
    odd_head, tbl_size, even_head, tbl_size);

out:
  big_free(odd_head);
  big_free(even_head);
  return statelist;
}

uint8_t lfsr_rollback_bit(struct Crypto1State *s, uint32_t in, int fb)
{
  int out;
  uint8_t ret;
  uint32_t t;
  s->odd &= 0xffffff;
  t = s->odd, s->odd = s->even, s->even = t;
  out = s->even & 1;
  out ^= LF_POLY_EVEN & (s->even >>= 1);
  out ^= LF_POLY_ODD & s->odd;
  out ^= !!in;
  out ^= (ret = filter(s->odd)) & !!fb;
  s->even |= parity(out) << 23;
  return ret;
}

uint8_t lfsr_rollback_byte(struct Crypto1State *s, uint32_t in, int fb)
{
  int i, ret = 0;
  for (i = 7; i >= 0; --i)
    ret |= lfsr_rollback_bit(s, BIT(in, i), fb) << i;
  return ret;
}

uint32_t lfsr_rollback_word(struct Crypto1State *s, uint32_t in, int fb)
{
  int i;
  uint32_t ret = 0;
  for (i = 31; i >= 0; --i)
    ret |= lfsr_rollback_bit(s, BEBIT(in, i), fb) << (i ^ 24);
  return ret;
}

static uint16_t *dist = 0;
int nonce_distance(uint32_t from, uint32_t to)
{
  uint16_t x, i;
  if(!dist) {
    dist = (uint16_t*)big_malloc(2 << 16);
    if(!dist) return -1;
    for (x = i = 1; i; ++i) {
      dist[(x & 0xff) << 8 | x >> 8] = i;
      x = x >> 1 | (x ^ x >> 2 ^ x >> 3 ^ x >> 5) << 15;
    }
  }
  return (65535 + dist[to >> 16] - dist[from >> 16]) % 65535;
}
