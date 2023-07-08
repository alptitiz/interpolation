#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The problem with strotoul and similar is that they consider negative numbers
// a valid input and provide no indication that the parsed number was negative -
// see man strtoul(3).
//
// We could use strtoimax and cast the result to size_t, but that would mean
// making the assumption that INTMAX_MAX is larger than SIZE_MAX, otherwise
// there are values which are out of range for the former (because it's signed)
// but not the latter.
//
// Since we are already implementing our own function, we change the signature:
//   numptr shall point to the first digit of the number (not the whitespace),
//   or be NULL if no number was read.
// This gives an easy way to find the amount of digits in the parsed number, and
// also lets us distinguish whether the parsing was valid (i.e. read only
// whitespace) if no digits were encountered.
//
// Other differences between our function and strtoul():
//  - it treats negative numbers like it would overflowing positive numbers
//  - it only considers numbers in base10
//  - if the first character after the whitespace is invalid, *endptr is set to
//    point at that character, not at nptr, even although no digits were
//    encountered
//  - and it expects that endptr (as well as numptr) is not NULL
size_t strtosizet(const char *nptr, const char **endptr, const char **numptr) {
  *numptr = NULL;
  for (*endptr = nptr; isspace(**endptr); (*endptr)++)
    ;
  if (**endptr == 0) {
    return 0;
  }

  size_t val = 0;
  if (**endptr == '+')
    (*endptr)++;
  else if (**endptr == '-')
    goto overflow;

  if (isdigit(**endptr))
    *numptr = *endptr;
  for (size_t oldval = 0; isdigit(**endptr); (*endptr)++) {
    if (__builtin_mul_overflow_p(10, val, (size_t)0))
      goto overflow;
    val *= 10;
    oldval = val;

    // We are allowed to subtract '0' for conversion of character to
    // corresponding digit because C11 standard guarantees:
    // > In both the source and execution basic character sets, the
    // > value of each character after 0 in the above list of decimal
    // > digits shall be one greater than the value of the previous.
    // See: https://port70.net/~nsz/c/c11/n1570.html#5.2.1p3
    val += (**endptr - '0');
    if (val < oldval)
      goto overflow;
    oldval = val;
  }

  return val;

overflow:
  errno = ERANGE;
  return SIZE_MAX;
}

// pow(x, y) from <math.h> works on doubles. We need such a function on
// integers. We also need to check for overflow, we do this by setting ERANGE if
// one occurs.
size_t int_pow(size_t base, size_t exp) {
  size_t acc = 1;

  for (; exp > 1; exp /= 2) {
    if (exp % 2 == 1)
      acc *= base;

    if (__builtin_mul_overflow_p(base, base, (size_t)0))
      errno = ERANGE;
    base = base * base;
  }

  if (__builtin_mul_overflow_p(acc, base, (size_t)0))
    errno = ERANGE;
  return acc * base;
}

// Since the calculations for input/output image size appear both in the main
// program and in the tests, we split them out into this file so we can easily
// verify that the overflow checks were done correctly everywhere.
//
// The following two functions return input/output image size if it didn't
// overflow, and SIZE_MAX otherwise. If an overflow happened, they set errno to
// ERANGE.

size_t input_imgsize(size_t width, size_t height) {
  // We allocate more bytes than needed.
  // We must guarantee that the width is divisble by 4 for scale3().
  // We must also guarantee that there are at least 5 extra bytes for scale5().
  // The reason for this is that two pixels are 6 bytes,
  // but SSE instructions can only read bytes in powers of 2, so they read 8
  // bytes. Reading in the last pixel (3 bytes) will cause _mm_loadu_64 to read
  // 5 bytes over the boundary, and it isn't possible to use _mm_loadu_32 on the
  // Rechnerhalle. So we must allocate *five* (instead of just two) additional
  // bytes prevent SSE from reading over the end and potentially causing a
  // segmentation fault.
  if (__builtin_mul_overflow_p(width, height, (size_t)0))
    goto overflow;
  size_t width_aligned = width + 8 - width % 4;
  size_t imgbuf_size = width_aligned * height;
  if (__builtin_mul_overflow_p(imgbuf_size, 3, (size_t)0))
    goto overflow;
  imgbuf_size *= 3; // Consider that each pixel needs 3 bytes to be represented
  if (__builtin_add_overflow_p(imgbuf_size, 5, (size_t)0))
    goto overflow;
  return imgbuf_size;

overflow:
  errno = ERANGE;
  return SIZE_MAX;
}

size_t output_imgsize(size_t width, size_t height, size_t scale_factor) {
  if (__builtin_mul_overflow_p(width, scale_factor, (size_t)0) ||
      __builtin_mul_overflow_p(height, scale_factor, (size_t)0))
    goto overflow;
  size_t width_out = width * scale_factor;
  size_t height_out = height * scale_factor;
  size_t size_out = width_out * height_out;
  if (__builtin_mul_overflow_p(width_out, height_out, (size_t)0) ||
      __builtin_mul_overflow_p(size_out, 3, (size_t)0))
    goto overflow;
  size_out *= 3;
  // More extra bytes due to the issues that _mm_storeu_64 otherwise causes when
  // calling scale5 with scale factor 1
  if (__builtin_add_overflow_p(size_out, 2, (size_t)0))
    goto overflow;
  size_out += 2;
  return size_out;

overflow:
  errno = ERANGE;
  return SIZE_MAX;
}
