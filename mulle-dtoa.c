/* A double-to-string conversion algorithm based on Schubfach (zmij variant).
 * Copyright (c) 2025 - present, Victor Zverovich
 * Copyright (c) 2025 - C conversion by Nat!
 * Distributed under the MIT license (see LICENSE).
 * 
 * This implementation ports the zmij algorithm (an optimized Schubfach variant)
 * from C++ to C while maintaining the original mulle-dtoa API.
 */

#include "mulle-dtoa.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct uint128 {
  uint64_t hi;
  uint64_t lo;
};

#ifdef __SIZEOF_INT128__
typedef unsigned __int128 uint128_t;
#else
typedef struct uint128 uint128_t;
#endif

static inline uint128_t umul128(uint64_t a, uint64_t b) {
#ifdef __SIZEOF_INT128__
    return (unsigned __int128)a * b;
#else
    uint64_t a_lo = (uint32_t)a, a_hi = a >> 32;
    uint64_t b_lo = (uint32_t)b, b_hi = b >> 32;
    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;
    uint64_t cy = (p0 >> 32) + (uint32_t)p1 + (uint32_t)p2;
    struct uint128 result = {p3 + (p1 >> 32) + (p2 >> 32) + (cy >> 32), p0 + (cy << 32)};
    return result;
#endif
}

static uint64_t umul192_upper64_modified(uint64_t pow10_hi, uint64_t pow10_lo, uint64_t scaled_sig) {
#ifdef __SIZEOF_INT128__
    unsigned __int128 x = umul128(pow10_lo, scaled_sig);
    uint64_t x_hi = (uint64_t)(x >> 64);
    unsigned __int128 y = umul128(pow10_hi, scaled_sig);
    uint64_t z = ((uint64_t)y >> 1) + x_hi;
    uint64_t result = (uint64_t)(y >> 64) + (z >> 63);
    const uint64_t mask = ((uint64_t)1 << 63) - 1;
    return result | (((z & mask) + mask) >> 63);
#else
    struct uint128 x = umul128(pow10_lo, scaled_sig);
    uint64_t x_hi = x.hi;
    struct uint128 y = umul128(pow10_hi, scaled_sig);
    uint64_t z = (y.lo >> 1) + x_hi;
    uint64_t result = y.hi + (z >> 63);
    const uint64_t mask = ((uint64_t)1 << 63) - 1;
    return result | (((z & mask) + mask) >> 63);
#endif
}

/* Power table */
static const struct uint128 pow10_significands[] = {
#include "pow10_table_data.inc"
};

/* Forward declarations */
static void write_mulle(char* buffer, uint64_t dec_sig, int dec_exp);
static char* write_significand(char* buffer, uint64_t value);
static void write2digits(char* buffer, uint32_t value);
static struct div_mod_result divmod100_3(uint32_t n);

void mulle_dtoa(double value, char* buffer) {
  uint64_t bits = 0;
  memcpy(&bits, &value, sizeof(value));
  *buffer = '-';
  buffer += bits >> 63;

  /* Early escape for 1.0 optimization */
  if (value == 1.0) {
    strcpy(buffer, "1");
    return;
  }
  if (value == -1.0) {
    strcpy(buffer - 1, "-1");
    return;
  }

  const int num_sig_bits = 52; /* std::numeric_limits<double>::digits - 1 */
  const int exp_mask = 0x7ff;
  int bin_exp = (int)(bits >> num_sig_bits) & exp_mask;

  const uint64_t implicit_bit = (uint64_t)1 << num_sig_bits;
  uint64_t bin_sig = bits & (implicit_bit - 1);

  int regular = bin_sig != 0;
  if (((bin_exp + 1) & exp_mask) <= 1) {
    if (bin_exp != 0) {
      memcpy(buffer, bin_sig == 0 ? "inf" : "nan", 4);
      return;
    }
    if (bin_sig == 0) {
      memcpy(buffer, "0", 2);
      return;
    }
    /* Handle subnormals */
    bin_sig ^= implicit_bit;
    bin_exp = 1;
    regular = 1;
  }
  bin_sig ^= implicit_bit;
  bin_exp -= num_sig_bits + 1023;

  /* Handle small integers */
  if ((bin_exp < 0) && (bin_exp >= -num_sig_bits)) {
    uint64_t f = bin_sig >> -bin_exp;
    if (f << -bin_exp == bin_sig) {
      write_mulle(buffer, f, 0);
      return;
    }
  }

  /* Shift the significand so that boundaries are integer */
  uint64_t bin_sig_shifted = bin_sig << 2;

  /* Compute the shifted boundaries of the rounding interval */
  uint64_t lower = bin_sig_shifted - (regular + 1);
  uint64_t upper = bin_sig_shifted + 2;

  const int log10_3_over_4_sig = -131008; /* Remove digit separator */
  const int log10_2_sig = 315653;         /* Remove digit separator */
  const int log10_2_exp = 20;

  assert(bin_exp >= -1334 && bin_exp <= 2620);
  int dec_exp = (bin_exp * log10_2_sig + (!regular) * log10_3_over_4_sig) >> log10_2_exp;

  const int dec_exp_min = -292;
  /* Replace structured binding auto [pow10_hi, pow10_lo] = ... */
  struct uint128 pow10_entry = pow10_significands[-dec_exp - dec_exp_min];
  uint64_t pow10_hi = pow10_entry.hi;
  uint64_t pow10_lo = pow10_entry.lo;

  const int log2_pow10_sig = 217707; /* Remove digit separator */
  const int log2_pow10_exp = 16;

  assert(dec_exp >= -350 && dec_exp <= 350);
  int pow10_bin_exp = -dec_exp * log2_pow10_sig >> log2_pow10_exp;

  int shift = bin_exp + pow10_bin_exp + 2;

  uint64_t bin_sig_lsb = bin_sig & 1;
  lower = umul192_upper64_modified(pow10_hi, pow10_lo, lower << shift) + bin_sig_lsb;
  upper = umul192_upper64_modified(pow10_hi, pow10_lo, upper << shift) - bin_sig_lsb;

  /* Single shorter candidate optimization */
  uint64_t shorter = 10 * ((upper >> 2) / 10);
  if ((shorter << 2) >= lower) {
    write_mulle(buffer, shorter, dec_exp);
    return;
  }

  uint64_t scaled_sig = umul192_upper64_modified(pow10_hi, pow10_lo, bin_sig_shifted << shift);
  uint64_t dec_sig_under = scaled_sig >> 2;
  uint64_t dec_sig_over = dec_sig_under + 1;

  /* Pick the closest candidate */
  int64_t cmp = (int64_t)(scaled_sig - ((dec_sig_under + dec_sig_over) << 1));
  int under_closer = cmp < 0 || (cmp == 0 && (dec_sig_under & 1) == 0);
  int under_in = (dec_sig_under << 2) >= lower;
  write_mulle(buffer, (under_closer && under_in) ? dec_sig_under : dec_sig_over, dec_exp);
}

/* Helper functions converted from zmij C++ */

static const char digits2_data[] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

static const char* digits2(size_t value) {
  return &digits2_data[value * 2];
}

static const char num_trailing_zeros[] =
    "\2\0\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0"
    "\1\0\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0"
    "\1\0\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0"
    "\1\0\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0"
    "\1\0\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0";

struct div_mod_result {
  uint32_t div;
  uint32_t mod;
};

/* Replace template with two functions */
static struct div_mod_result divmod100_3(uint32_t value) {
  const int exp = 19;
  assert(value < 1000);
  const int sig = (1 << exp) / 100 + 1;
  uint32_t div = (value * sig) >> exp;
  struct div_mod_result result = {div, value - div * 100};
  return result;
}

static struct div_mod_result divmod100_4(uint32_t value) {
  const int exp = 19;
  assert(value < 10000);
  const int sig = (1 << exp) / 100 + 1;
  uint32_t div = (value * sig) >> exp;
  struct div_mod_result result = {div, value - div * 100};
  return result;
}

static void write2digits(char* buffer, uint32_t value) {
  memcpy(buffer, digits2(value), 2);
}

static char* write4digits(char* buffer, uint32_t value) {
  /* Replace auto [aa, bb] = divmod100<4>(value) */
  struct div_mod_result dm = divmod100_4(value);
  uint32_t aa = dm.div;
  uint32_t bb = dm.mod;
  write2digits(buffer + 0, aa);
  write2digits(buffer + 2, bb);
  return buffer + 4 - num_trailing_zeros[bb] - (bb == 0) * num_trailing_zeros[aa];
}

static char* write_significand(char* buffer, uint64_t value) {
  /* Each digit is denoted by a letter so value is abbccddeeffgghhii where digit a can be zero */
  uint32_t abbccddee = (uint32_t)(value / 100000000ULL); /* Remove digit separators */
  uint32_t ffgghhii = (uint32_t)(value % 100000000ULL);
  uint32_t abbcc = abbccddee / 10000;
  uint32_t ddee = abbccddee % 10000;
  uint32_t abb = abbcc / 100;
  uint32_t cc = abbcc % 100;
  /* Replace auto [a, bb] = divmod100<3>(abb) */
  struct div_mod_result dm_abb = divmod100_3(abb);
  uint32_t a = dm_abb.div;
  uint32_t bb = dm_abb.mod;

  *buffer = (char)('0' + a);
  buffer += a != 0;
  write2digits(buffer + 0, bb);
  write2digits(buffer + 2, cc);
  buffer += 4;

  if (ffgghhii == 0) {
    if (ddee != 0) return write4digits(buffer, ddee);
    return buffer - num_trailing_zeros[cc] - (cc == 0) * num_trailing_zeros[bb];
  }
  /* Replace auto [dd, ee] = divmod100<4>(ddee) */
  struct div_mod_result dm_ddee = divmod100_4(ddee);
  uint32_t dd = dm_ddee.div;
  uint32_t ee = dm_ddee.mod;
  uint32_t ffgg = ffgghhii / 10000;
  uint32_t hhii = ffgghhii % 10000;
  /* Replace auto [ff, gg] = divmod100<4>(ffgg) */
  struct div_mod_result dm_ffgg = divmod100_4(ffgg);
  uint32_t ff = dm_ffgg.div;
  uint32_t gg = dm_ffgg.mod;
  write2digits(buffer + 0, dd);
  write2digits(buffer + 2, ee);
  write2digits(buffer + 4, ff);
  write2digits(buffer + 6, gg);
  if (hhii != 0) return write4digits(buffer + 8, hhii);
  return buffer + 8 - num_trailing_zeros[gg] - (gg == 0) * num_trailing_zeros[ff];
}

static int uint64_to_string(uint64_t value, char* buffer) {
  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return 1;
  }
  
  char temp[32];
  int len = 0;
  while (value > 0) {
    temp[len++] = '0' + (value % 10);
    value /= 10;
  }
  
  /* Reverse the digits */
  for (int i = 0; i < len; i++) {
    buffer[i] = temp[len - 1 - i];
  }
  buffer[len] = '\0';
  return len;
}

static void write_mulle(char* buffer, uint64_t dec_sig, int dec_exp) {
  /* Normalize to shortest representation */
  int trailing_zeros = 0;
  while (dec_sig % 10 == 0 && dec_sig > 0) {
    dec_sig /= 10;
    trailing_zeros++;
  }
  dec_exp += trailing_zeros;
  
  /* Calculate the actual decimal exponent */
  char temp[32];
  int sig_len = uint64_to_string(dec_sig, temp);
  int actual_exp = dec_exp + sig_len - 1;
  
  /* Choose shortest format */
  if (actual_exp >= -4 && actual_exp <= 6) {
    /* Use fixed-point notation */
    if (actual_exp >= 0) {
      /* Integer or decimal >= 1: 12.2, 122, etc. */
      if (actual_exp < sig_len - 1) {
        /* Need decimal point: 12.2 */
        int int_digits = actual_exp + 1;
        memcpy(buffer, temp, int_digits);
        buffer[int_digits] = '.';
        memcpy(buffer + int_digits + 1, temp + int_digits, sig_len - int_digits);
        buffer[sig_len + 1] = '\0';
      } else {
        /* Pure integer: 122 */
        memcpy(buffer, temp, sig_len);
        for (int i = 0; i < actual_exp - sig_len + 1; i++) {
          buffer[sig_len + i] = '0';
        }
        buffer[sig_len + actual_exp - sig_len + 1] = '\0';
      }
    } else {
      /* Decimal < 1: 0.122 */
      buffer[0] = '0';
      buffer[1] = '.';
      for (int i = 0; i < -actual_exp - 1; i++) {
        buffer[2 + i] = '0';
      }
      memcpy(buffer + 2 + (-actual_exp - 1), temp, sig_len);
      buffer[2 + (-actual_exp - 1) + sig_len] = '\0';
    }
  } else {
    /* Use scientific notation */
    buffer[0] = temp[0];
    if (sig_len > 1) {
      buffer[1] = '.';
      memcpy(buffer + 2, temp + 1, sig_len - 1);
      buffer[sig_len + 1] = 'e';
      buffer[sig_len + 2] = actual_exp >= 0 ? '+' : '-';
      int abs_exp = actual_exp >= 0 ? actual_exp : -actual_exp;
      if (abs_exp >= 100) {
        buffer[sig_len + 3] = '0' + (abs_exp / 100);
        buffer[sig_len + 4] = '0' + ((abs_exp / 10) % 10);
        buffer[sig_len + 5] = '0' + (abs_exp % 10);
        buffer[sig_len + 6] = '\0';
      } else {
        buffer[sig_len + 3] = '0' + (abs_exp / 10);
        buffer[sig_len + 4] = '0' + (abs_exp % 10);
        buffer[sig_len + 5] = '\0';
      }
    } else {
      buffer[1] = '.';
      buffer[2] = 'e';
      buffer[3] = actual_exp >= 0 ? '+' : '-';
      int abs_exp = actual_exp >= 0 ? actual_exp : -actual_exp;
      if (abs_exp >= 100) {
        buffer[4] = '0' + (abs_exp / 100);
        buffer[5] = '0' + ((abs_exp / 10) % 10);
        buffer[6] = '0' + (abs_exp % 10);
        buffer[7] = '\0';
      } else {
        buffer[4] = '0' + (abs_exp / 10);
        buffer[5] = '0' + (abs_exp % 10);
        buffer[6] = '\0';
      }
    }
  }
}

struct mulle_dtoa_decimal mulle_dtoa_decompose(double value) {
    struct mulle_dtoa_decimal result = {0};
    
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(value));
    
    result.sign = (bits >> 63) & 1;
    
    const int num_sig_bits = 52;
    const int exp_mask = 0x7ff;
    int bin_exp = (int)(bits >> num_sig_bits) & exp_mask;
    uint64_t bin_sig = bits & (((uint64_t)1 << num_sig_bits) - 1);
    
    /* Handle special cases */
    if (bin_exp == 0x7ff) {
        result.special = bin_sig == 0 ? 1 : 2; /* inf : nan */
        return result;
    }
    
    if (bin_exp == 0 && bin_sig == 0) {
        result.special = 3; /* zero */
        return result;
    }
    
    /* Run the same zmij algorithm to get decimal representation */
    const uint64_t implicit_bit = (uint64_t)1 << num_sig_bits;
    int regular = bin_sig != 0;
    
    if (((bin_exp + 1) & exp_mask) <= 1) {
        if (bin_exp != 0) {
            result.special = bin_sig == 0 ? 1 : 2;
            return result;
        }
        bin_sig ^= implicit_bit;
        bin_exp = 1;
        regular = 1;
    }
    bin_sig ^= implicit_bit;
    bin_exp -= num_sig_bits + 1023;
    
    /* Use zmij algorithm to get decimal significand and exponent */
    uint64_t bin_sig_shifted = bin_sig << 2;
    uint64_t lower = bin_sig_shifted - (regular + 1);
    uint64_t upper = bin_sig_shifted + 2;
    
    const int log10_3_over_4_sig = -131008;
    const int log10_2_sig = 315653;
    const int log10_2_exp = 20;
    
    int dec_exp = (bin_exp * log10_2_sig + (!regular) * log10_3_over_4_sig) >> log10_2_exp;
    
    const int dec_exp_min = -292;
    struct uint128 pow10_entry = pow10_significands[-dec_exp - dec_exp_min];
    uint64_t pow10_hi = pow10_entry.hi;
    uint64_t pow10_lo = pow10_entry.lo;
    
    const int log2_pow10_sig = 217707;
    const int log2_pow10_exp = 16;
    int pow10_bin_exp = -dec_exp * log2_pow10_sig >> log2_pow10_exp;
    int shift = bin_exp + pow10_bin_exp + 2;
    
    uint64_t bin_sig_lsb = bin_sig & 1;
    lower = umul192_upper64_modified(pow10_hi, pow10_lo, lower << shift) + bin_sig_lsb;
    upper = umul192_upper64_modified(pow10_hi, pow10_lo, upper << shift) - bin_sig_lsb;
    
    uint64_t shorter = 10 * ((upper >> 2) / 10);
    if ((shorter << 2) >= lower) {
        result.significand = shorter;
        result.exponent = dec_exp;
        result.special = 0;
        return result;
    }
    
    uint64_t scaled_sig = umul192_upper64_modified(pow10_hi, pow10_lo, bin_sig_shifted << shift);
    uint64_t dec_sig_under = scaled_sig >> 2;
    uint64_t dec_sig_over = dec_sig_under + 1;
    
    int64_t cmp = (int64_t)(scaled_sig - ((dec_sig_under + dec_sig_over) << 1));
    int under_closer = cmp < 0 || (cmp == 0 && (dec_sig_under & 1) == 0);
    int under_in = (dec_sig_under << 2) >= lower;
    
    result.significand = (under_closer && under_in) ? dec_sig_under : dec_sig_over;
    result.exponent = dec_exp;
    result.special = 0;
    return result;
}
