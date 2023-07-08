// clock_gettime requires this macro to compile; the alternative would have been
// to change GCC flags from -std=c17 to -std=gnu17, but it is unclear whether we
// are allowed to do so. See: man clock_gettime(2)
//      man feature_test_macros(7)
#define _POSIX_C_SOURCE 199309L

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

void subtract_timespec(struct timespec *tres, struct timespec t1,
                       struct timespec t2) {
  time_t carry = 0;
  if (t1.tv_nsec > t2.tv_nsec) {
    tres->tv_nsec = 1000000000 - (t1.tv_nsec - t2.tv_nsec);
    carry = 1;
  } else {
    tres->tv_nsec = t2.tv_nsec - t1.tv_nsec;
  }

  tres->tv_sec = t2.tv_sec - t1.tv_sec - carry;
}

int timing_loop(struct timespec *total, bool do_timing, size_t timing_repeats,
                void (*fun)(const uint8_t *, size_t, size_t, size_t, uint8_t *),
                const uint8_t *img, size_t width, size_t height,
                size_t scale_factor, uint8_t *result) {
  struct timespec start;
  struct timespec stop;
  errno = 0;
  if (!do_timing) {
    (*fun)(img, width, height, scale_factor, result);
  } else {
    int res1 = clock_gettime(CLOCK_MONOTONIC, &start);
    for (size_t i = 0; i < timing_repeats; i++) {
      (*fun)(img, width, height, scale_factor, result);
    }
    int res2 = clock_gettime(CLOCK_MONOTONIC, &stop);
    if (res1 || res2) {
      perror("Error getting time");
      return 1;
    }
    subtract_timespec(total, start, stop);
  }
  if (errno == ENOMEM) {
    fprintf(stderr, "Error while scaling: Could not allocate memory.\n");
    if (do_timing)
      fprintf(stderr, "Scale function terminated by error, timing results will "
                      "be inaccurate.\n");
    return 1;
  }
  return 0;
}
