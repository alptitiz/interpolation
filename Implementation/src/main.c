#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "file_parsing.h"
#include "scale.h"
#include "test.h"
#include "timing.h"
#include "util.h"

// Since we're passing the almost same parameters in every switch case, define a
// macro
#define TIMING_LOOP(FUN)                                                       \
  if (timing_loop(&total, do_timing, timing_repeats, FUN, inimg.img,           \
                  inimg.width, inimg.height, scale_factor, scaled_img))        \
    goto cleanup;

const char *help_text = "\
Usage: %s [options] file.ppm\n\
Valid options are:\n\
--time|-B [repeats]\n\
\tMeasure how much time the scaling took. The call to the scaling function is iterated [repeats] times, by default 100.\n\
--help|-h\n\
\tShow this help message and exit.\n\
--out|-o <filename>\n\
\tWrite the image to <filename>. Without this option, it is written to out.ppm in the current directory.\n\
--scale_factor|-f <factor>\n\
\tScale the image by <factor>.\n\
--test|-t\n\
\tInstead of scaling an image, run the automated tests and exit.\n\
--version|-V <version>\n\
\tSelect a specific implementation of the scale function.\n";

// Wrapper function around strtosizet() from util.c that handles parsing errors
// Return value:
//   0 if parsing successful
//   1 otherwise
int strtosizet_wrapper(char *src, size_t *dest, const char *name) {
  const char *endptr, *numptr;
  errno = 0;
  *dest = strtosizet(src, &endptr, &numptr);
  if (errno == ERANGE) {
    fprintf(stderr,
            "Error processing --%s: Argument out of range for size_t.\n", name);
    return 1;
  } else if (*endptr != 0) {
    fprintf(stderr, "Error processing --%s: Argument not an integer.\n", name);
    return 1;
  } else if (numptr == NULL) {
    fprintf(stderr, "Error processing --%s: Argument empty.\n", name);
    return 1;
  }

  return 0;
}

int main(int argc, char **argv) {



  size_t scale_factor = 1;
  size_t use_version = 0;
  bool do_timing = false;
  bool run_tests = false;
  size_t timing_repeats = 100;
  char *name_in;
  char *name_out = "out.ppm";

  FILE *infile = NULL;
  FILE *outfile = NULL;
  struct img_st inimg = {0, 0, NULL};
  uint8_t *scaled_img = NULL;

  // Process options with getopt()
  int option_index = 0;
  const char *optstring =
      ":B::ho:f:V:"; // : at the beginning of optstring causes getopt() to
                     // distinguish between missing argument for option and
                     // unknown option; see man getopt(1)
  static struct option long_options[] = {
      {"time", required_argument , NULL, 'B'},
      {"help", no_argument, NULL, 'h'},
      {"out", required_argument, NULL, 'o'},
      {"scale_factor", required_argument, NULL, 'f'},
      {"test", no_argument, NULL, 't'},
      {"version", required_argument, NULL, 'V'},
      {0, 0, NULL, 0}};
  for (char c = getopt_long(argc, argv, optstring, long_options, &option_index);
       c != -1;
       c = getopt_long(argc, argv, optstring, long_options, &option_index)) {

    switch (c) {
    case 'B':
      do_timing = true;
      if (optarg && strtosizet_wrapper(optarg, &timing_repeats, "time"))
        return EXIT_FAILURE;
      break;
    case 'h':
      printf(help_text, argv[0]);
      return EXIT_SUCCESS;
    case 'o':
      if (!strlen(optarg)) {
        fprintf(stderr, "Error processing --out: Filename empty.\n");
        return EXIT_FAILURE;
      }
      name_out = optarg;
      break;
    case 'f':
      if (strtosizet_wrapper(optarg, &scale_factor, "scale_factor"))
        return EXIT_FAILURE;
      break;
    case 't':
      run_tests = true;
      break;
    case 'V':
      if (strtosizet_wrapper(optarg, &use_version, "version"))
        return EXIT_FAILURE;
      if (use_version > MAX_IMPLEMENTATION) {
        fprintf(stderr, "No such implementation: -V%lu\n", use_version);
        return EXIT_FAILURE;
      }
      break;
    case ':':
      fprintf(stderr, "Error: missing argument for -%c\n", optopt);
      return EXIT_FAILURE;
    default:
      if (optopt != 0)
        fprintf(stderr, "Unknown option: -%c\n", optopt);
      else
        fprintf(stderr, "Unknown option2: %s\n", argv[optind - 1]);
      return EXIT_FAILURE;
    }
    option_index = 0;
  }

  if (run_tests) {
    int parser_failed_tests = test_parser();
    int batch_failed_tests = test_batch();
    int hard_coded_failed_tests = test_hard_coded();

    printf("\n");
    if (!parser_failed_tests)
      printf("Parser tests successful.\n");
    else
      printf("Failed %d parser test(s).\n", parser_failed_tests);

    if (batch_failed_tests) {
      fprintf(stderr, "Failed scale tests: %d test(s) failed.\n",
              batch_failed_tests);
    } else {
      printf("Scale tests sucessful.\n");
    }

    if (hard_coded_failed_tests) {
      fprintf(stderr, "Failed hardcoded tests: %d test(s) failed.\n",
              hard_coded_failed_tests);
    } else {
      printf("Hardcoded tests sucessful.\n");
    }

    if (parser_failed_tests || batch_failed_tests)
      return EXIT_FAILURE;
    else
      return EXIT_SUCCESS;
  }

  // Get the input name
  if (optind >= argc) {
    fprintf(stderr, "Error: No input file name specified.\n");
    fprintf(stderr, "Use: %s input_file_name\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (!strlen(argv[optind])) {
    fprintf(stderr, "Error: Input file name empty.\n");
    return EXIT_FAILURE;
  }

  name_in = argv[optind];

  // Open files for IO
  // We do not need to check whether infile is a regular file - if it isn't, the
  // file read operations that we do later will fail and we can handle that.
  infile = fopen(name_in, "r");
  if (!infile) {
    perror("Error opening input file");
    goto cleanup;
  }
  // If the outfile isn't a regular file that we can write to, fopen() will
  // fail, we do not need any extra checks here.
  outfile = fopen(
      name_out,
      "w"); // Open the outfile now. Wouldn't want to do all the calculations
            // just to find out we can't write them to outfile.
  if (!outfile) {
    perror("Error opening output file");
    goto cleanup;
  }

  // Parse the image from input file
  if (parse_file(infile, &inimg))
    goto cleanup;
  fclose(infile);
  infile = NULL; // to prevent it from being closed again if we goto cleanup

  // Calculate the amount of memory needed for output image and allocate it
  errno = 0;
  size_t size_out = output_imgsize(inimg.width, inimg.height, scale_factor);
  if (errno == ERANGE)
    goto malloc_error;

  if (inimg.width * inimg.height * scale_factor != 0) {
    scaled_img = malloc(size_out);
    if (!scaled_img)
      goto malloc_error;

    struct timespec total;

    if (use_version == 0) {
      if (scale_factor <= 16 && inimg.width > 1) {
        // Fast implementation, but only works for scale_factor <= 16
        use_version = 4;
      } else {
        use_version = 1;
      }
    }

    if (timing_loop(&total, do_timing, timing_repeats,
                    scale_funs[use_version - 1], inimg.img, inimg.width,
                    inimg.height, scale_factor, scaled_img))
      goto cleanup;
    if (do_timing)
      printf("Took %ld.%03lds for %lu iterations.\n", total.tv_sec,
             total.tv_nsec / 1000000, timing_repeats);
  } else {
    scaled_img = NULL;
    if (do_timing)
      fprintf(stderr, "Note: --time is set, but image size is 0. An image of "
                      "size 0 is copied without running any scaling function, "
                      "so there are no timing results.\n");
  }

  if (write_img(outfile, inimg.width * scale_factor,
                inimg.height * scale_factor, scaled_img)) {
    fprintf(stderr, "Error writing to output file.\n");
    goto cleanup;
  }
  fclose(outfile);

  if (inimg.img)
    free(inimg.img);
  if (scaled_img)
    free(scaled_img);

  return EXIT_SUCCESS;

malloc_error:
  fprintf(stderr, "Error allocating memory.\n");
cleanup:
  if (inimg.img)
    free(inimg.img);
  if (scaled_img)
    free(scaled_img);
  if (infile)
    fclose(infile);
  if (outfile)
    fclose(outfile);
  return EXIT_FAILURE;
}
