// Calculates t2 - t1 and stores result in tres
extern void subtract_timespec(struct timespec *tres, struct timespec t1,
                              struct timespec t2);

// Explanation of the signature:
//   struct timespec *total:
//     pointer to timespec into which to write result
//   bool do_timing:
//     if false, don't do timing and the loop, just call the function
//   void (*fun)(...):
//     pointer to a function that takes the same arguments as scale()
//     (to be used with implementations of scale())
//   ...:
//     parameters to be passed to (*fun)
extern int
timing_loop(struct timespec *total, bool do_timing, size_t timing_repeats,
            void (*fun)(const uint8_t *, size_t, size_t, size_t, uint8_t *),
            const uint8_t *img, size_t width, size_t height,
            size_t scale_factor, uint8_t *result);
