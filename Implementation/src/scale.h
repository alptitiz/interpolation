#ifndef CHANNELS
#define CHANNELS 3
#endif

extern void scale1(const uint8_t *img, size_t width, size_t height,
                   size_t scale_factor, uint8_t *result);
extern void scale2(const uint8_t *img, size_t width, size_t height,
                   size_t scale_factor, uint8_t *result);
extern void scale3(const uint8_t *img, size_t width, size_t height,
                   size_t scale_factor, uint8_t *result);
extern void scale4(const uint8_t *img, size_t width, size_t height,
                   size_t scale_factor, uint8_t *result);
extern void scale_naive(const uint8_t *img, size_t width, size_t height,
                        size_t scale_factor, uint8_t *result);

// Change these when adding a new scale() implementation:
//  - increment MAX_IMPLEMENTATION
//  - Add the implementation to the two arrays
#ifndef MAX_IMPLEMENTATION
#define MAX_IMPLEMENTATION 4
#endif

__attribute__((unused)) static void (*scale_funs[])(const uint8_t *, size_t,
                                                    size_t, size_t,
                                                    uint8_t *) = {
    scale1, scale2, scale3, scale4};
