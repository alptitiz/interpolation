#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_parsing.h"
#include "scale.h"
#include "test.h"
#include "util.h"

#define MAX_PATH_LENGTH 200
#define MAX_NUM_IMG 4
#define MAX_HARDCODED_SF 3
#define BOUNDARY_CHECK(impl) (impl == 0 || impl == 3)

int iterate_functions(size_t scale_factor, uint8_t *img, size_t width,
                      size_t height, uint8_t *expected, size_t num_img);
bool compare(uint8_t *result, uint8_t *expected, size_t height, size_t width,
             size_t scale_factor, bool check_boundary);

// Helper function to run a single test.
// expect_values:
//   first bit:
//     whether to check for a specific width (stored in expect_width)
//   second bit:
//     whether to check for a specific height (stored in expect_height)
//   third bit:
//     whether to check for specific data (stored in expect_data)
int parser_test(char *file_name, char *description, enum parse_err expect_res,
                uint8_t expect_values, size_t expect_width,
                size_t expect_height, uint8_t *expect_data) {
  FILE *fp;
  struct img_st dest;
  enum parse_err res;

  fp = fopen(file_name, "r");
  if (!fp) {
    printf("Error: Failed to open test file %s\n", file_name);
    res = READ_ERR;
    goto failure;
  }

  printf("%s... ", description);

  res = parse_file_h(fp, &dest);
  if (res != expect_res)
    goto failure;
  if ((expect_values & 0x01) && dest.width != expect_width)
    goto failure;
  if ((expect_values & 0x02) && dest.height != expect_height)
    goto failure;
  if (expect_values & 0x04) {
    if (dest.img && expect_data) {
      if (memcmp(dest.img, expect_data, dest.width * dest.height * 3))
        goto failure;
    } else if (dest.img != expect_data)
      goto failure; // If we expect NULL and get NULL, that's valid, but not if
                    // only one of the given arrays is valid.
  }

  printf("OK.\n");
  if (res == PARSE_OK)
    free(dest.img);
  fclose(fp);
  return 0;

failure:
  printf("Failed.\n");
  if (res == PARSE_OK)
    free(dest.img);
  if (fp)
    fclose(fp);
  return 1;
}

int test_parser(void) {
  int tf = 0; // amount of failed tests

  // A very small sample image that we use to check whether the contents of some
  // files were read correctly
  uint8_t sample_image[] = {0xff, 0x00, 0x00, 0x00, 0xff, 0xff,
                            0x00, 0x00, 0xff, 0xff, 0x00, 0xff};

  // Read two larger rasters used for testing
  uint8_t jmt_raster[512 * 512 * 3];
  uint8_t lena_raster[512 * 512 * 3];
  FILE *fp1 = fopen("test/parse/johnmuirtrail-raster", "r");
  if (!fp1)
    goto files_error;
  if (fread(jmt_raster, 1, 512 * 512 * 3, fp1) < 512 * 512 * 3)
    goto files_error;
  fclose(fp1);
  FILE *fp2 = fopen("test/parse/lena-raster", "r");
  if (!fp2)
    goto files_error;
  if (fread(lena_raster, 1, 512 * 512 * 3, fp2) < 512 * 512 * 3)
    goto files_error;
  fclose(fp2);

  tf += parser_test("test/parse/0by0-p3.ppm",
                    "Testing whether parsing a P3 file of size 0x0 works",
                    PARSE_OK, 7, 0, 0, NULL);

  tf += parser_test("test/parse/0by0-p6.ppm",
                    "Testing whether parsing a P6 file of size 0x0 works",
                    PARSE_OK, 7, 0, 0, NULL);

  tf += parser_test("test/parse/0by5.ppm",
                    "Testing whether parsing a P6 file of size 0x5 works",
                    PARSE_OK, 7, 0, 5, NULL);

  tf += parser_test("test/parse/5by0.ppm",
                    "Testing whether parsing a P6 file of size 5x0 works",
                    PARSE_OK, 7, 5, 0, NULL);

  tf += parser_test("test/parse/comments.ppm",
                    "Testing whether parser can handle comments", PARSE_OK, 7,
                    2, 2, sample_image);

  tf += parser_test("test/parse/johnmuirtrail.ppm",
                    "Testing whether parsing johnmuirtrail.ppm works", PARSE_OK,
                    7, 512, 512, jmt_raster);

  tf += parser_test("test/parse/lena.ppm",
                    "Testing whether parsing lena.ppm works", PARSE_OK, 7, 512,
                    512, lena_raster);

  tf += parser_test(
      "test/parse/inconvenient-whitespace.ppm",
      "Testing whether parser can handle arbitrary whitespace in the header",
      PARSE_OK, 3, 120, 120, NULL);

  tf += parser_test(
      "test/parse/header-spaced-p3.ppm",
      "Testing whether parser can handle header not ending with \\n (P3)",
      PARSE_OK, 7, 2, 2, sample_image);

  tf += parser_test(
      "test/parse/header-spaced-p6.ppm",
      "Testing whether parser can handle header not ending with \\n (P6)",
      PARSE_OK, 7, 2, 2, sample_image);

  tf += parser_test("test/parse/init-dos.ppm",
                    "Testing whether parser can handle DOS line endings",
                    PARSE_OK, 7, 2, 2, sample_image);

  tf += parser_test("test/parse/init-p3-noeol.ppm",
                    "Testing whether parse can handle last number of P3 image "
                    "ending with null-byte (not whitespace)",
                    PARSE_OK, 7, 2, 2, sample_image);

  tf += parser_test("/dev/zero",
                    "Testing whether parser rejects file without magic number",
                    NO_MAGIC_NUM, 0, 0, 0, NULL);

  tf += parser_test(
      "test/parse/random-junk.ppm",
      "Testing whether parser rejects random junk beginning with magic number",
      PARSE_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/negative-width.ppm",
                    "Testing whether parser rejects negative width",
                    OUT_OF_RANGE_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/negative-height.ppm",
                    "Testing whether parser rejects negative height",
                    OUT_OF_RANGE_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/negative-maxval.ppm",
                    "Testing whether parser rejects negative maxval",
                    OUT_OF_RANGE_ERR, 0, 0, 0, NULL);

  tf += parser_test(
      "test/parse/not-enough-memory.ppm",
      "Testing for proper error handling if can't allocate enough memory",
      MALLOC_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/out-of-range.ppm",
                    "Testing for proper error handling if image width out of "
                    "range for size_t",
                    OUT_OF_RANGE_ERR, 0, 0, 0, NULL);

  tf += parser_test(
      "test/parse/overflow-mul-1.ppm",
      "Testing whether parser detects overflow in dest->width * dest->height",
      MALLOC_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/overflow-mul-2.ppm",
                    "Testing whether parser detects overflow in 3 * "
                    "dest->width * dest->height",
                    MALLOC_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/overflow-add.ppm",
                    "Testing whether parser detects overflow in 3 * "
                    "dest->width * dest->height + 2",
                    MALLOC_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/too-short-p3.ppm",
                    "Testing whether parser can handle P3 file having less "
                    "pixels than the header said",
                    READ_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/too-short-p6.ppm",
                    "Testing whether parser can handle P6 file having less "
                    "pixels than the header said",
                    READ_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/overflow-split-number.ppm",
                    "Testing whether parser detects number out of range when "
                    "number is split across two fgets() calls",
                    OUT_OF_RANGE_ERR, 0, 0, 0, NULL);

  tf += parser_test("test/parse/nullbytes-in-header.ppm",
                    "Testing whether parser rejects null bytes in header",
                    PARSE_ERR, 0, 0, 0, NULL);
  return tf;

files_error:
  printf("Failed to read a file necessary to run the tests, exiting.\n");
  return 1;
}

int test_batch(void) {
  printf("\nScale function tests\n");
  int fail = 0;

  // Scale factors to be tested
  const int sf_len = 3;
  int scale_factors[] = {1, 12, 17};

  char path[MAX_PATH_LENGTH];

  for (size_t num_img = 0; num_img < MAX_NUM_IMG; num_img++) {
    // Read the input file
    snprintf(path, MAX_PATH_LENGTH, "test/scale/%zu.ppm", num_img);
    FILE *infile = fopen(path, "r");
    if (!infile) {
      fprintf(stderr, "Test failed: Error opening input file %zu.ppm\n",
              num_img);
      ++fail;
      continue;
    }

    // Parse input file into buffer
    struct img_st inimg = {0, 0, NULL};
    if (parse_file(infile, &inimg)) {
      fprintf(stderr, "Test failed: Error reading input file %zu.ppm\n",
              num_img);
      ++fail;
      continue;
    }

    // Iterate over all scale factors and test the scale function on the images
    for (int i = 0; i < sf_len; i++) {
      int s = scale_factors[i];

      // Allocate buffer for expected result
      size_t size_out = output_imgsize(inimg.width, inimg.height, s);
      uint8_t *expected = malloc(size_out);
      if (errno == ERANGE || !expected) {
        fprintf(stderr,
                "Test failed: Error allocating memory for output image.\n");
        ++fail;
        continue;
      }
      // Calculate the expected result
      scale_naive(inimg.img, inimg.width, inimg.height, s, expected);

      fail += iterate_functions(s, inimg.img, inimg.width, inimg.height,
                                expected, num_img);
      free(expected);
    }

    free(inimg.img);
    fclose(infile);
  }
  return fail;
}

int test_hard_coded() {
  // Image that will be scaled
  uint8_t img[12] = {255, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 255};

  // Expected results for hardcoded test for each scale_factor (sf=1, sf=2,
  // sf=3)
  uint8_t arr0[] = {255, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 255};
  uint8_t arr1[] = {255, 0, 0,   127, 127, 127, 0,   255, 255, 0,   255, 255,
                    127, 0, 127, 127, 63,  191, 127, 127, 255, 127, 127, 255,
                    0,   0, 255, 127, 0,   255, 255, 0,   255, 255, 0,   255,
                    0,   0, 255, 127, 0,   255, 255, 0,   255, 255, 0,   255};
  uint8_t arr2[] = {255, 0,   0,   170, 85,  85,  85,  170, 170, 0,   255, 255,
                    0,   255, 255, 0,   255, 255, 170, 0,   85,  141, 56,  141,
                    113, 113, 198, 85,  170, 255, 85,  170, 255, 85,  170, 255,
                    85,  0,   170, 113, 28,  198, 141, 56,  226, 170, 85,  255,
                    170, 85,  255, 170, 85,  255, 0,   0,   255, 85,  0,   255,
                    170, 0,   255, 255, 0,   255, 255, 0,   255, 255, 0,   255,
                    0,   0,   255, 85,  0,   255, 170, 0,   255, 255, 0,   255,
                    255, 0,   255, 255, 0,   255, 0,   0,   255, 85,  0,   255,
                    170, 0,   255, 255, 0,   255, 255, 0,   255, 255, 0,   255};
  uint8_t *hardcoded[3] = {arr0, arr1, arr2};

  printf("\nHard coded tests\n");

  int fail = 0;
  for (size_t sf = 1; sf <= MAX_HARDCODED_SF; sf++) {
    fail += iterate_functions(sf, img, 2, 2, hardcoded[sf - 1], 0);
  }
  return fail;
}

// This function changes the result array:
//   If result pixel matches expected pixel, it's replaced with green
//   If result pixel doesn't match expected pixel, it's replaced with red
//   If result pixel is an edge pixel and check_boundary is false, it's replaced
//   with blue
bool compare(uint8_t *result, uint8_t *expected, size_t height, size_t width,
             size_t scale_factor, bool check_boundary) {
  int fail = 0;

  for (size_t y = 0; y < (check_boundary ? height : (height - scale_factor));
       y++) {
    for (size_t x = 0; x < (check_boundary ? width : (width - scale_factor));
         x++) {
      bool bad_pixel = false;
      for (size_t i = 0; i < CHANNELS; i++) {
        if (expected[CHANNELS * (y * width + x) + i] !=
            result[CHANNELS * (y * width + x) + i]) {
          bad_pixel = true;
          ++fail;
        }
      }
      if (bad_pixel) {
        // Colour wrong pixel red
        result[CHANNELS * (y * width + x)] = 255;
        result[CHANNELS * (y * width + x) + 1] = 0;
        result[CHANNELS * (y * width + x) + 2] = 0;
      } else {
        // Colour correct pixel green
        result[CHANNELS * (y * width + x)] = 0;
        result[CHANNELS * (y * width + x) + 1] = 255;
        result[CHANNELS * (y * width + x) + 2] = 0;
      }
    }
    if (!check_boundary) {
      for (size_t x = width - scale_factor; x < width; x++) {
        // We're in the last column, colour edge pixel blue
        result[CHANNELS * (y * width + x)] = 0;
        result[CHANNELS * (y * width + x) + 1] = 0;
        result[CHANNELS * (y * width + x) + 2] = 255;
      }
    }
  }
  if (!check_boundary) {
    for (size_t x = 0; x < width; x++) {
      // We're in the last row, colour edge pixel blue
      result[CHANNELS * ((height - 1) * width + x)] = 0;
      result[CHANNELS * ((height - 1) * width + x) + 1] = 0;
      result[CHANNELS * ((height - 1) * width + x) + 2] = 255;
    }
  }
  return fail > 0;
}

int iterate_functions(size_t scale_factor, uint8_t *img, size_t width,
                      size_t height, uint8_t *expected, size_t num_img) {
  char path_out[MAX_PATH_LENGTH];
  int fail = 0;

  // Allocate buffer for results of the scale functions
  size_t size_out = output_imgsize(width, height, scale_factor);
  uint8_t *result = malloc(size_out);
  if (!result) {
    fprintf(stderr, "Test failed: Error allocating memory for output image.\n");
    ++fail;
    goto cleanup;
  }

  // Test the scale functions
  for (int j = 0; j < MAX_IMPLEMENTATION; j++) {
    // scale4 only handles scale_factors up to 16
    if (j == 3 && scale_factor > 16)
      continue;

    errno = 0;
    scale_funs[j](img, width, height, scale_factor, result);
    if (errno == ENOMEM) {
      fprintf(stderr, "Error during scaling: Failed to allocate memory.\n");
      printf(
          "Test failed: Img: %zu.ppm, Function: scale%d, scale_factor: %zu\n",
          num_img, j + 1, scale_factor);
      ++fail;
      continue;
    }

    bool check_boundary = BOUNDARY_CHECK(j);

    // Compare actual and expected results, write image produced by compare() if
    // they differ
    if (compare(result, expected, height * scale_factor, width * scale_factor,
                scale_factor, check_boundary)) {
      snprintf(path_out, MAX_PATH_LENGTH, "test/out/scale%d_%zu_%zu.ppm", j + 1,
               scale_factor, num_img);

      FILE *outfile = fopen(path_out, "w");
      if (!outfile) {
        perror("Error opening output file");
        printf(
            "Test failed: Img: %zu.ppm, Function: scale%d, scale_factor: %zu\n",
            num_img, j + 1, scale_factor);
        ++fail;
        continue;
      }

      size_t width_out = width * scale_factor;
      size_t height_out = height * scale_factor;
      if (write_img(outfile, width_out, height_out, result)) {
        fprintf(stderr, "Error writing to output file.\n");
        printf("Test failed: Img: %zu, Function: scale%d, scale_factor: %zu\n",
               num_img, j + 1, scale_factor);
        ++fail;
        fclose(outfile);
        continue;
      }
      printf(
          "Test failed: Img: %zu.ppm, Function: scale%d, scale_factor: %zu\n",
          num_img, j + 1, scale_factor);
      ++fail;
      fclose(outfile);
      continue;
    }

    printf("Test passed: Img: %zu.ppm, Function: scale%d, scale_factor: %zu\n",
           num_img, j + 1, scale_factor);
  }

cleanup:
  if (result)
    free(result);
  return fail;
}
