/*
 * ct.h - primitive constant-time condivise (erano duplicate in bfmax.h e
 * steroids.h: ct_nz64/ct_nz64_mask, ct_ctz_u64/ct_ctz64).
 * La barriera asm impedisce al compilatore di ritrasformare le selezioni
 * con maschere in branch.
 */
#pragma once

#include <stdint.h>
#include "types.h"

/* 0xFF..FF if x != 0, 0 otherwise */
static INLINE uint64_t ct_nz64_mask(uint64_t x)
{
   uint64_t m = (uint64_t)0 - ((x | ((uint64_t)0 - x)) >> 63);
   __asm__("" : "+r"(m));
   return m;
}

/* 1 if x != 0, 0 otherwise */
static INLINE uint32_t ct_is_nz64(uint64_t x)
{
   return (uint32_t)((x | ((uint64_t)0 - x)) >> 63);
}

/* trailing zeros of x (x != 0) without bsf/tzcnt */
static INLINE uint32_t ct_ctz64(uint64_t x)
{
   uint64_t y = x & ((uint64_t)0 - x);
   uint32_t n = 0;
   n |= ct_is_nz64(y & 0xAAAAAAAAAAAAAAAAULL) << 0;
   n |= ct_is_nz64(y & 0xCCCCCCCCCCCCCCCCULL) << 1;
   n |= ct_is_nz64(y & 0xF0F0F0F0F0F0F0F0ULL) << 2;
   n |= ct_is_nz64(y & 0xFF00FF00FF00FF00ULL) << 3;
   n |= ct_is_nz64(y & 0xFFFF0000FFFF0000ULL) << 4;
   n |= ct_is_nz64(y & 0xFFFFFFFF00000000ULL) << 5;
   return n;
}

