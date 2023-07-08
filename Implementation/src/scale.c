#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <xmmintrin.h> // SSE
#include <emmintrin.h> // SSE2
#include <pmmintrin.h> // SSE3
#include <tmmintrin.h> // SSSE3
#include <smmintrin.h> // SSE4.1

#include "scale.h"

void scale1(const uint8_t *img, size_t width, size_t height,
            size_t scale_factor, uint8_t *result) {

  size_t width_out = width * scale_factor;

  double s2 = 1.0 / ((double)(scale_factor * scale_factor));

  const uint8_t *q00 = img;
  const uint8_t *q01 = q00 + CHANNELS;
  const uint8_t *q10 = q00 + CHANNELS * width;
  const uint8_t *q11 = q10 + CHANNELS;

  size_t *c0 = malloc(scale_factor * scale_factor * sizeof(size_t *));
  size_t *c1 = malloc(scale_factor * scale_factor * sizeof(size_t *));
  size_t *c2 = malloc(scale_factor * scale_factor * sizeof(size_t *));
  size_t *c3 = malloc(scale_factor * scale_factor * sizeof(size_t *));
  if (!c0 || !c1 || !c2 || !c3) {
    if (c0)
      free(c0);
    if (c1)
      free(c1);
    if (c2)
      free(c2);
    if (c3)
      free(c3);
    errno = ENOMEM;
    return;
  }

  for (size_t y = 0; y < scale_factor; y++) {
    for (size_t x = 0; x < scale_factor; x++) {
      c0[y * scale_factor + x] = (scale_factor - x) * (scale_factor - y);
      c1[y * scale_factor + x] = (scale_factor - x) * y;
      c2[y * scale_factor + x] = x * (scale_factor - y);
      c3[y * scale_factor + x] = x * y;
    }
  }

  // let xi, eta be the original image coordinate system, let x,y be the scaled
  // image local coordinate system
  for (size_t eta = 0; eta < height - 1; eta++) {
    for (size_t xi = 0; xi < width - 1; xi++) {
      for (size_t y = 0; y < scale_factor; y++) {
        for (size_t x = 0; x < scale_factor; x++) {
          for (size_t i = 0; i < CHANNELS; i++) {
            result[CHANNELS * (scale_factor * (width_out * eta + xi) +
                               width_out * y + x) +
                   i] =
                (uint8_t)(s2 * (c0[y * scale_factor + x] *
                                    q00[CHANNELS * (eta * width + xi) + i] +
                                c1[y * scale_factor + x] *
                                    q10[CHANNELS * (eta * width + xi) + i] +
                                c2[y * scale_factor + x] *
                                    q01[CHANNELS * (eta * width + xi) + i] +
                                c3[y * scale_factor + x] *
                                    q11[CHANNELS * (eta * width + xi) + i]));
          }
        }
      }
    }
    // last column
    for (size_t y = 0; y < scale_factor; y++) {
      for (size_t x = 0; x < scale_factor; x++) {
        for (size_t i = 0; i < CHANNELS; i++) {
          result[CHANNELS * (scale_factor * (width_out * eta + width - 1) +
                             width_out * y + x) +
                 i] =
              (uint8_t)(s2 *
                        (c0[y * scale_factor + x] *
                             q00[CHANNELS * (eta * width + width - 1) + i] +
                         c1[y * scale_factor + x] *
                             q10[CHANNELS * (eta * width + width - 1) + i] +
                         c2[y * scale_factor + x] *
                             q00[CHANNELS * (eta * width + width - 1) + i] +
                         c3[y * scale_factor + x] *
                             q10[CHANNELS * (eta * width + width - 1) + i]));
        }
      }
    }
  }
  // last row
  for (size_t xi = 0; xi < width - 1; xi++) {
    for (size_t y = 0; y < scale_factor; y++) {
      for (size_t x = 0; x < scale_factor; x++) {
        for (size_t i = 0; i < CHANNELS; i++) {
          result[CHANNELS * (scale_factor * (width_out * (height - 1) + xi) +
                             width_out * y + x) +
                 i] =
              (uint8_t)(s2 *
                        (c0[y * scale_factor + x] *
                             q00[CHANNELS * ((height - 1) * width + xi) + i] +
                         c1[y * scale_factor + x] *
                             q00[CHANNELS * ((height - 1) * width + xi) + i] +
                         c2[y * scale_factor + x] *
                             q01[CHANNELS * ((height - 1) * width + xi) + i] +
                         c3[y * scale_factor + x] *
                             q01[CHANNELS * ((height - 1) * width + xi) + i]));
        }
      }
    }
  }
  // right bottom corner
  for (size_t y = 0; y < scale_factor; y++) {
    for (size_t x = 0; x < scale_factor; x++) {
      for (size_t i = 0; i < CHANNELS; i++) {
        result[CHANNELS *
                   (scale_factor * (width_out * (height - 1) + (width - 1)) +
                    width_out * y + x) +
               i] = q00[CHANNELS * ((height - 1) * width + (width - 1)) + i];
      }
    }
  }
  free(c0);
  free(c1);
  free(c2);
  free(c3);
}

void scale2(const uint8_t *img, size_t width, size_t height,
            size_t scale_factor, uint8_t *result) {
  size_t width_out = width * scale_factor;
  for (size_t x = 0; x < (height - 1) * scale_factor; x++) {
    for (size_t y = 0; y < (width - 1) * scale_factor; y++) {

      size_t orX = x / scale_factor;
      size_t orY = y / scale_factor;

      size_t c00 = (scale_factor - (y % scale_factor)) *
                   (scale_factor - (x % scale_factor));
      size_t cS0 = (x % scale_factor) * (scale_factor - (y % scale_factor));
      size_t c0S = (y % scale_factor) * (scale_factor - (x % scale_factor));
      size_t cSS = (x % scale_factor) * (y % scale_factor);

      uint8_t p_00_r = img[width * 3 * orX + 3 * orY];
      uint8_t p_00_g = img[width * 3 * orX + 3 * orY + 1];
      uint8_t p_00_b = img[width * 3 * orX + 3 * orY + 2];
      uint8_t p_s0_r = img[width * 3 * (orX + 1) + 3 * orY];
      uint8_t p_s0_g = img[width * 3 * (orX + 1) + 3 * orY + 1];
      uint8_t p_s0_b = img[width * 3 * (orX + 1) + 3 * orY + 2];
      uint8_t p_0s_r = img[width * 3 * orX + 3 * (orY + 1)];
      uint8_t p_0s_g = img[width * 3 * orX + 3 * (orY + 1) + 1];
      uint8_t p_0s_b = img[width * 3 * orX + 3 * (orY + 1) + 2];
      uint8_t p_ss_r = img[width * 3 * (orX + 1) + 3 * (orY + 1)];
      uint8_t p_ss_g = img[width * 3 * (orX + 1) + 3 * (orY + 1) + 1];
      uint8_t p_ss_b = img[width * 3 * (orX + 1) + 3 * (orY + 1) + 2];

      int redT = ((c00) * (p_00_r)) + ((cS0) * (p_s0_r)) + ((c0S) * (p_0s_r)) +
                 ((cSS) * (p_ss_r));

      int greenT = ((c00) * (p_00_g)) + ((cS0) * (p_s0_g)) +
                   ((c0S) * (p_0s_g)) + ((cSS) * (p_ss_g));

      int blueT = ((c00) * (p_00_b)) + ((cS0) * (p_s0_b)) + ((c0S) * (p_0s_b)) +
                  ((cSS) * (p_ss_b));

      result[x * width * scale_factor * 3 + 3 * y] =
          (uint8_t)(redT / (scale_factor * scale_factor));
      result[x * width * scale_factor * 3 + (3 * y) + 1] =
          (uint8_t)(greenT / (scale_factor * scale_factor));
      result[x * width * scale_factor * 3 + (3 * y) + 2] =
          (uint8_t)(blueT / (scale_factor * scale_factor));
    }
  }

  // last column
  for (size_t eta = 0; eta < height - 1; eta++) {
    for (size_t y = 0; y < scale_factor; y++) {
      for (size_t x = 0; x < scale_factor; x++) {
        for (size_t i = 0; i < CHANNELS; i++) {
          result[CHANNELS * (scale_factor * (width_out * eta + width - 1) +
                             width_out * y + x) +
                 i] = 0;
        }
      }
    }
  }
  // last row
  for (size_t xi = 0; xi < width - 1; xi++) {
    for (size_t y = 0; y < scale_factor; y++) {
      for (size_t x = 0; x < scale_factor; x++) {
        for (size_t i = 0; i < CHANNELS; i++) {
          result[CHANNELS * (scale_factor * (width_out * (height - 1) + xi) +
                             width_out * y + x) +
                 i] = 0;
        }
      }
    }
  }
  // right bottom corner
  for (size_t y = 0; y < scale_factor; y++) {
    for (size_t x = 0; x < scale_factor; x++) {
      for (size_t i = 0; i < CHANNELS; i++) {
        result[CHANNELS *
                   (scale_factor * (width_out * (height - 1) + (width - 1)) +
                    width_out * y + x) +
               i] = 0;
      }
    }
  }
}

void scale3(const uint8_t *img, size_t width, size_t height,
            size_t scale_factor, uint8_t *result) {
  size_t width_out = width * scale_factor;

  double s2 = 1.0 / ((double)(scale_factor * scale_factor));

  for (size_t eta = 0; eta < height - 1; eta++) {

    size_t y0 = eta * scale_factor;

    for (size_t xi = 0; xi < width - 1; xi += 4) {

      size_t x0 = xi * scale_factor;
      size_t x2 = (xi + 1) * scale_factor;
      size_t x3 = (xi + 2) * scale_factor;
      size_t x4 = (xi + 3) * scale_factor;
      __m128i p00 =
          _mm_loadu_si128((const __m128i *)(img + width * 3 * eta + 3 * xi));
      __m128i p01 = _mm_loadu_si128(
          (const __m128i *)(img + width * 3 * (eta + 1) + 3 * xi));

      __m128i mcRed =
          _mm_set_epi32(_mm_extract_epi8(p00, 0), _mm_extract_epi8(p00, 3),
                        _mm_extract_epi8(p01, 0), _mm_extract_epi8(p01, 3));
      __m128i mcGreen =
          _mm_set_epi32(_mm_extract_epi8(p00, 1), _mm_extract_epi8(p00, 4),
                        _mm_extract_epi8(p01, 1), _mm_extract_epi8(p01, 4));
      __m128i mcBlue =
          _mm_set_epi32(_mm_extract_epi8(p00, 2), _mm_extract_epi8(p00, 5),
                        _mm_extract_epi8(p01, 2), _mm_extract_epi8(p01, 5));

      __m128i mcRed2 =
          _mm_set_epi32(_mm_extract_epi8(p00, 3), _mm_extract_epi8(p00, 6),
                        _mm_extract_epi8(p01, 3), _mm_extract_epi8(p01, 6));
      __m128i mcGreen2 =
          _mm_set_epi32(_mm_extract_epi8(p00, 4), _mm_extract_epi8(p00, 7),
                        _mm_extract_epi8(p01, 4), _mm_extract_epi8(p01, 7));
      __m128i mcBlue2 =
          _mm_set_epi32(_mm_extract_epi8(p00, 5), _mm_extract_epi8(p00, 8),
                        _mm_extract_epi8(p01, 5), _mm_extract_epi8(p01, 8));

      __m128i mcRed3 =
          _mm_set_epi32(_mm_extract_epi8(p00, 6), _mm_extract_epi8(p00, 9),
                        _mm_extract_epi8(p01, 6), _mm_extract_epi8(p01, 9));
      __m128i mcGreen3 =
          _mm_set_epi32(_mm_extract_epi8(p00, 7), _mm_extract_epi8(p00, 10),
                        _mm_extract_epi8(p01, 7), _mm_extract_epi8(p01, 10));
      __m128i mcBlue3 =
          _mm_set_epi32(_mm_extract_epi8(p00, 8), _mm_extract_epi8(p00, 11),
                        _mm_extract_epi8(p01, 8), _mm_extract_epi8(p01, 11));

      __m128i mcRed4 =
          _mm_set_epi32(_mm_extract_epi8(p00, 9), _mm_extract_epi8(p00, 12),
                        _mm_extract_epi8(p01, 9), _mm_extract_epi8(p01, 12));
      __m128i mcGreen4 =
          _mm_set_epi32(_mm_extract_epi8(p00, 10), _mm_extract_epi8(p00, 13),
                        _mm_extract_epi8(p01, 10), _mm_extract_epi8(p01, 13));
      __m128i mcBlue4 =
          _mm_set_epi32(_mm_extract_epi8(p00, 11), _mm_extract_epi8(p00, 14),
                        _mm_extract_epi8(p01, 11), _mm_extract_epi8(p01, 14));

      for (size_t y = 0; y < scale_factor; y++) {

        for (size_t x = 0; x < scale_factor; x++) {

          size_t c0 = ((scale_factor - x) * (scale_factor - y));
          size_t c1 = ((scale_factor - x) * y);
          size_t c2 = (x * (scale_factor - y));
          size_t c3 = (x * y);

          __m128i ms00 = _mm_set_epi32(c0, c2, c1, c3);

          __m128i resRed = _mm_mullo_epi32(mcRed, ms00);
          __m128i resGreen = _mm_mullo_epi32(mcGreen, ms00);
          __m128i resBlue = _mm_mullo_epi32(mcBlue, ms00);

          __m128i resRed2 = _mm_mullo_epi32(mcRed2, ms00);
          __m128i resGreen2 = _mm_mullo_epi32(mcGreen2, ms00);
          __m128i resBlue2 = _mm_mullo_epi32(mcBlue2, ms00);

          __m128i resRed3 = _mm_mullo_epi32(mcRed3, ms00);
          __m128i resGreen3 = _mm_mullo_epi32(mcGreen3, ms00);
          __m128i resBlue3 = _mm_mullo_epi32(mcBlue3, ms00);

          __m128i resRed4 = _mm_mullo_epi32(mcRed4, ms00);
          __m128i resGreen4 = _mm_mullo_epi32(mcGreen4, ms00);
          __m128i resBlue4 = _mm_mullo_epi32(mcBlue4, ms00);

          resRed = _mm_hadd_epi32(resRed, resRed);
          resRed = _mm_hadd_epi32(resRed, resRed);
          resGreen = _mm_hadd_epi32(resGreen, resGreen);
          resGreen = _mm_hadd_epi32(resGreen, resGreen);
          resBlue = _mm_hadd_epi32(resBlue, resBlue);
          resBlue = _mm_hadd_epi32(resBlue, resBlue);

          resRed2 = _mm_hadd_epi32(resRed2, resRed2);
          resRed2 = _mm_hadd_epi32(resRed2, resRed2);
          resGreen2 = _mm_hadd_epi32(resGreen2, resGreen2);
          resGreen2 = _mm_hadd_epi32(resGreen2, resGreen2);
          resBlue2 = _mm_hadd_epi32(resBlue2, resBlue2);
          resBlue2 = _mm_hadd_epi32(resBlue2, resBlue2);

          resRed3 = _mm_hadd_epi32(resRed3, resRed3);
          resRed3 = _mm_hadd_epi32(resRed3, resRed3);
          resGreen3 = _mm_hadd_epi32(resGreen3, resGreen3);
          resGreen3 = _mm_hadd_epi32(resGreen3, resGreen3);
          resBlue3 = _mm_hadd_epi32(resBlue3, resBlue3);
          resBlue3 = _mm_hadd_epi32(resBlue3, resBlue3);

          resRed4 = _mm_hadd_epi32(resRed4, resRed4);
          resRed4 = _mm_hadd_epi32(resRed4, resRed4);
          resGreen4 = _mm_hadd_epi32(resGreen4, resGreen4);
          resGreen4 = _mm_hadd_epi32(resGreen4, resGreen4);
          resBlue4 = _mm_hadd_epi32(resBlue4, resBlue4);
          resBlue4 = _mm_hadd_epi32(resBlue4, resBlue4);

          result[CHANNELS * (width_out * (y0 + y) + (x0 + x)) + 0] =
              (uint8_t)(_mm_extract_epi32(resRed, 0) * s2);
          result[CHANNELS * (width_out * (y0 + y) + (x0 + x)) + 1] =
              (uint8_t)(_mm_extract_epi32(resGreen, 0) * s2);
          result[CHANNELS * (width_out * (y0 + y) + (x0 + x)) + 2] =
              (uint8_t)(_mm_extract_epi32(resBlue, 0) * s2);

          result[CHANNELS * (width_out * (y0 + y) + (x2 + x)) + 0] =
              (uint8_t)(_mm_extract_epi32(resRed2, 0) * s2);
          result[CHANNELS * (width_out * (y0 + y) + (x2 + x)) + 1] =
              (uint8_t)(_mm_extract_epi32(resGreen2, 0) * s2);
          result[CHANNELS * (width_out * (y0 + y) + (x2 + x)) + 2] =
              (uint8_t)(_mm_extract_epi32(resBlue2, 0) * s2);

          result[CHANNELS * (width_out * (y0 + y) + (x3 + x)) + 0] =
              (uint8_t)(_mm_extract_epi32(resRed3, 0) * s2);
          result[CHANNELS * (width_out * (y0 + y) + (x3 + x)) + 1] =
              (uint8_t)(_mm_extract_epi32(resGreen3, 0) * s2);
          result[CHANNELS * (width_out * (y0 + y) + (x3 + x)) + 2] =
              (uint8_t)(_mm_extract_epi32(resBlue3, 0) * s2);

          result[CHANNELS * (width_out * (y0 + y) + (x4 + x)) + 0] =
              (uint8_t)(_mm_extract_epi32(resRed4, 0) * s2);
          result[CHANNELS * (width_out * (y0 + y) + (x4 + x)) + 1] =
              (uint8_t)(_mm_extract_epi32(resGreen4, 0) * s2);
          result[CHANNELS * (width_out * (y0 + y) + (x4 + x)) + 2] =
              (uint8_t)(_mm_extract_epi32(resBlue4, 0) * s2);
        }
      }
    }
  }

  // last column
  for (size_t eta = 0; eta < height - 1; eta++) {
    for (size_t y = 0; y < scale_factor; y++) {
      for (size_t x = 0; x < scale_factor; x++) {
        for (size_t i = 0; i < CHANNELS; i++) {
          result[CHANNELS * (scale_factor * (width_out * eta + width - 1) +
                             width_out * y + x) +
                 i] = 0;
        }
      }
    }
  }

  // last row
  for (size_t xi = 0; xi < width - 1; xi++) {
    for (size_t y = 0; y < scale_factor; y++) {
      for (size_t x = 0; x < scale_factor; x++) {
        for (size_t i = 0; i < CHANNELS; i++) {
          result[CHANNELS * (scale_factor * (width_out * (height - 1) + xi) +
                             width_out * y + x) +
                 i] = 0;
        }
      }
    }
  }
  // right bottom corner
  for (size_t y = 0; y < scale_factor; y++) {
    for (size_t x = 0; x < scale_factor; x++) {
      for (size_t i = 0; i < CHANNELS; i++) {
        result[CHANNELS *
                   (scale_factor * (width_out * (height - 1) + (width - 1)) +
                    width_out * y + x) +
               i] = 0;
      }
    }
  }
}

// NOTE: Works only with scale_factor <= 16
void scale4(const uint8_t *img, size_t width, size_t height,
            size_t scale_factor, uint8_t *result) {
  const size_t px_width =
      width * 3; // The width if you count 3 bytes for each pixel
  const size_t px_width_out = px_width * scale_factor;
  const uint8_t sf = (uint8_t)scale_factor;
  const bool small_enough = scale_factor <= 12;

  // To initialize the xmm registers when the loop begins, and update them in
  // the loop
  const __m128i start_syy = _mm_set_epi16(0, 0, sf, sf, sf, sf, sf, sf);
  const __m128i start_sxx = _mm_set_epi16(0, 0, 0, 0, 0, sf, sf, sf);
  const __m128i just_ones = _mm_set_epi16(1, 1, 1, 1, 1, 1, 1, 1);
  const __m128i plusminus = _mm_set_epi16(0, 0, 1, 1, 1, -1, -1, -1);
  // To place the second pixel (epi16) of the result into a new xmm register
  const __m128i shufmsk =
      _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 11, 10, 9, 8, 7, 6);
  // To "convert" (rather, take the relevant bytes) epi32 back to epi8
  const __m128i cvtmsk =
      _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 8, 4, 0);
  // To avoid overwriting the beginning of the next line when handling last
  // column
  const __m128i writemsk =
      _mm_set_epi8(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, -1, -1, -1);
  // To multiply the calculation result with it
  const __m128 factorxmm = _mm_set_ps1(1.0 / (scale_factor * scale_factor));
  const __m128 factorxmm2 = _mm_set_ps1(1.0 / scale_factor);

  // pxvals1: stores "top" two pixel values from img
  // pxvals2: stores "bottom" two pixel values from img
  // syy: stores (s-y) (s-y); sy: stores y y
  // sxx: stores (s-x) x
  // mres: stores result of multiplication
  __m128i pxvals1, pxvals2, sxx, syy, sy, mres1, mres2;
  for (size_t yglobal = 0; yglobal < height - 1; yglobal += 1) {
    for (size_t xglobal = 0; xglobal < px_width - 3; xglobal += 3) {
      // Note: _mm_loadu_si64 also sets bits 127:64 of the xmm register to 0
      pxvals1 = _mm_loadu_si64(img + yglobal * px_width + xglobal);
      pxvals2 = _mm_loadu_si64(img + (yglobal + 1) * px_width + xglobal);

      // "Convert" uint8_t to uint16_t by adding zero byte between each byte
      pxvals1 = _mm_unpacklo_epi8(pxvals1, _mm_setzero_si128());
      pxvals2 = _mm_unpacklo_epi8(pxvals2, _mm_setzero_si128());

      syy = start_syy;
      sy = _mm_setzero_si128();
      for (uint8_t y = 0; y < sf; y++) {
        sxx = start_sxx;
        for (uint8_t x = 0; x < sf; x++) {
          // Do the calculation:
          //   (s-y)(s-x)P(0,0) + (s-y)xP(0,s) + y(s-x)P(s,0) + yxP(s,s)
          // Result will be in the lower 96 bit of mres1, as three 32-bit
          // integers
          mres1 = _mm_mullo_epi16(pxvals1, syy);
          mres2 = _mm_mullo_epi16(pxvals2, sy);
          mres1 = _mm_add_epi16(mres1, mres2);
          mres1 = _mm_mullo_epi16(mres1, sxx);
          mres2 =
              _mm_shuffle_epi8(mres1, shufmsk); // store second pixel in mres2
          if (small_enough) {
            mres1 = _mm_add_epi16(mres1, mres2);
            mres1 = _mm_cvtepu16_epi32(mres1);
          } else {
            mres1 = _mm_cvtepu16_epi32(mres1);
            mres2 = _mm_cvtepu16_epi32(mres2);
            mres1 = _mm_add_epi32(mres1, mres2);
          }

          // Finally, multiply with the scale factor!
          __m128 mres_flt = _mm_cvtepi32_ps(mres1);
          mres1 = _mm_cvttps_epi32(_mm_mul_ps(mres_flt, factorxmm));

          // We have it as 32-bit integers. But we need 8-bit integers.
          mres1 = _mm_shuffle_epi8(mres1, cvtmsk);

          // Write the resulting 3 bytes (and additional five that will get
          // overwritten) of the pixel into the result
          _mm_storeu_si64(result + (yglobal * scale_factor + y) * px_width_out +
                              scale_factor * xglobal + 3 * x,
                          mres1);

          // Update sxx
          sxx = _mm_add_epi16(sxx, plusminus);
        }
        syy = _mm_sub_epi16(syy, just_ones);
        sy = _mm_add_epi16(sy, just_ones);
      }
    }

    // In the last column, there is nothing left to interpolate horizontally.
    // Just interpolate vertically and copy across the columns.
    pxvals1 = _mm_shuffle_epi8(
        pxvals1, shufmsk); // move second pixel so that it becomes first pixel
    pxvals2 = _mm_shuffle_epi8(pxvals2, shufmsk);
    syy = start_syy;
    sy = _mm_setzero_si128();
    for (size_t y = 0; y < scale_factor; y++) {
      mres1 = _mm_mullo_epi16(pxvals1, syy);
      mres2 = _mm_mullo_epi16(pxvals2, sy);
      mres1 = _mm_add_epi16(mres1, mres2);
      mres2 = _mm_shuffle_epi8(mres1, shufmsk);
      mres1 = _mm_add_epi16(mres1, mres2);
      mres1 = _mm_cvtepu16_epi32(mres1);

      __m128 mres_flt = _mm_cvtepi32_ps(mres1);
      mres1 = _mm_cvttps_epi32(_mm_mul_ps(mres_flt, factorxmm2));
      mres1 = _mm_shuffle_epi8(mres1, cvtmsk);

      if (scale_factor > 1) {
        for (size_t x = px_width_out - 3 * scale_factor; x < px_width_out - 6;
             x += 3) {
          _mm_storeu_si64(
              result + (yglobal * scale_factor + y) * px_width_out + x, mres1);
        }
        _mm_maskmoveu_si128(mres1, writemsk,
                            (char *)result +
                                (yglobal * scale_factor + y) * px_width_out +
                                px_width_out - 6);
      }
      memcpy(result + (yglobal * scale_factor + y) * px_width_out +
                 px_width_out - 3,
             &mres1, 3);
      syy = _mm_sub_epi16(syy, just_ones);
      sy = _mm_add_epi16(sy, just_ones);
    }
  }

  // In the last row, there is nothing left to interpolate vertically.
  // Just interpolate horizontally and copy across the columns.
  const uint8_t *last_line = img + (height - 1) * px_width;
  for (size_t x_in = 0; x_in < px_width - 3; x_in += 3) {
    pxvals1 = _mm_loadu_si64(last_line + x_in);
    pxvals1 = _mm_unpacklo_epi8(pxvals1, _mm_setzero_si128());
    sxx = start_sxx;
    for (size_t x = 0; x < 3 * scale_factor; x += 3) {
      mres1 = _mm_mullo_epi16(pxvals1, sxx);
      mres2 = _mm_shuffle_epi8(mres1, shufmsk);
      mres1 = _mm_add_epi16(mres1, mres2);
      mres1 = _mm_cvtepu16_epi32(mres1);
      __m128 mres_flt = _mm_cvtepi32_ps(mres1);
      mres1 = _mm_cvttps_epi32(_mm_mul_ps(mres_flt, factorxmm2));
      mres1 = _mm_shuffle_epi8(mres1, cvtmsk);
      for (size_t y = (height - 1) * scale_factor * px_width_out;
           y < height * scale_factor * px_width_out; y += px_width_out) {
        _mm_storeu_si64(result + y + x_in * scale_factor + x, mres1);
      }
      sxx = _mm_add_epi16(sxx, plusminus);
    }
  }

  // In the last pixel, there is nothing to interpolate at all.
  // Just copy it into the scaled version.
  //
  // It would have been better to use _mm_loadu_si32 (because it would only read
  // one byte beyond the array end, while _mm_loadu_si64 reads five bytes
  // beyond. But unfortunately, Rechnerhalle uses an outdated GCC which doesn't
  // work with _mm_loadu_si32, and we're required to support the Rechnerhalle
  // setup... See the following link:
  //   https://zulip.in.tum.de/#narrow/stream/1102-GRA-22S-Projects/topic/gcc.20Version.3F
  //
  // We did an attempt to make the best of it by at duplicating the pixel so
  // that we can write two at a time using _mm_storeu_si64, but measurements
  // showed no improvement, so we're staying with this approach.
  //
  // The reason for using SSE instructions at all, instead of just memcpy , is
  // that performance seems to be slightly better with the SSE instructions.
  pxvals1 = _mm_loadu_si64(img + height * px_width - 3);
  for (size_t y = 0; y < scale_factor; y++) {
    if (scale_factor > 1) {
      for (size_t x = 0; x < 3 * scale_factor - 6; x += 3) {
        _mm_storeu_si64(result + (height - 1) * scale_factor * px_width_out +
                            y * px_width_out + px_width_out - 3 * scale_factor +
                            x,
                        pxvals1);
      }
      // Note: _mm_maskmoveu_si128 still produces an invalid read and write with
      // valgrind. Using memcpy instead.
      memcpy(result + (height - 1) * scale_factor * px_width_out +
                 y * px_width_out + px_width_out - 6,
             &pxvals1, 3);
    }
    memcpy(result + (height - 1) * scale_factor * px_width_out +
               y * px_width_out + px_width_out - 3,
           &pxvals1, 3);
  }
}

void scale_naive(const uint8_t *img, size_t width, size_t height,
                 size_t scale_factor, uint8_t *result) {
  double s2inv = 1.0 / (scale_factor * scale_factor);
  size_t width_out = width * scale_factor;
  for (size_t eta = 0; eta < height - 1; eta++) {
    for (size_t xi = 0; xi < width - 1; xi++) {
      for (size_t y = 0; y < scale_factor; y++) {
        for (size_t x = 0; x < scale_factor; x++) {
          if (x == 0 && y == 0) {
            for (size_t i = 0; i < CHANNELS; i++) {
              result[CHANNELS * ((eta * scale_factor + y) * width_out +
                                 (xi * scale_factor + x)) +
                     i] = img[CHANNELS * (eta * width + xi) + i];
            }
          } else {
            for (size_t i = 0; i < CHANNELS; i++) {
              int p0 = img[CHANNELS * (eta * width + xi) + i];
              int p1 = img[CHANNELS * (eta * width + xi + 1) + i];
              int p2 = img[CHANNELS * ((eta + 1) * width + xi) + i];
              int p3 = img[CHANNELS * ((eta + 1) * width + xi + 1) + i];
              int c0 = (scale_factor - y) * (scale_factor - x);
              int c1 = (scale_factor - y) * x;
              int c2 = y * (scale_factor - x);
              int c3 = x * y;
              result[CHANNELS * ((eta * scale_factor + y) * width_out +
                                 (xi * scale_factor + x)) +
                     i] = s2inv * (c0 * p0 + c1 * p1 + c2 * p2 + c3 * p3);
            }
          }
        }
      }
    }
    // last column
    for (size_t y = 0; y < scale_factor; y++) {
      for (size_t x = 0; x < scale_factor; x++) {
        for (size_t i = 0; i < CHANNELS; i++) {
          int c0 = (scale_factor - y) * (scale_factor - x);
          int c1 = (scale_factor - y) * x;
          int c2 = y * (scale_factor - x);
          int c3 = x * y;
          int p0 = img[CHANNELS * (eta * width + (width - 1)) + i];
          int p2 = img[CHANNELS * ((eta + 1) * width + (width - 1)) + i];

          result[CHANNELS * (scale_factor * (width_out * eta + width - 1) +
                             width_out * y + x) +
                 i] =
              (uint8_t)(s2inv * (c0 * p0 + c1 * p0 + c2 * p2 + c3 * p2));
        }
      }
    }
  }
  // last row
  for (size_t xi = 0; xi < width - 1; xi++) {
    for (size_t y = 0; y < scale_factor; y++) {
      for (size_t x = 0; x < scale_factor; x++) {
        for (size_t i = 0; i < CHANNELS; i++) {
          int c0 = (scale_factor - y) * (scale_factor - x);
          int c1 = (scale_factor - y) * x;
          int c2 = y * (scale_factor - x);
          int c3 = x * y;
          int p0 = img[CHANNELS * ((height - 1) * width + xi) + i];
          int p1 = img[CHANNELS * ((height - 1) * width + xi + 1) + i];

          result[CHANNELS * (scale_factor * (width_out * (height - 1) + xi) +
                             width_out * y + x) +
                 i] =
              (uint8_t)(s2inv * (c0 * p0 + c1 * p1 + c2 * p0 + c3 * p1));
        }
      }
    }
  }
  // bottom right corner
  for (size_t y = 0; y < scale_factor; y++) {
    for (size_t x = 0; x < scale_factor; x++) {
      for (size_t i = 0; i < CHANNELS; i++) {
        int p0 = img[CHANNELS * ((height - 1) * width + (width - 1)) + i];
        result[CHANNELS *
                   (scale_factor * (width_out * (height - 1) + (width - 1)) +
                    width_out * y + x) +
               i] = p0;
      }
    }
  }
}
