#ifndef IMG_ST_H
#define IMG_ST_H

// NOTE: We do not store colour depth.
//       Since task statement says 24-bit color, we can assume depth is 255.
struct img_st {
  size_t width;
  size_t height;
  uint8_t *img;
};
#endif

#ifndef PARSE_ERR_H
#define PARSE_ERR_H
enum parse_err {
  PARSE_OK = 0,
  READ_ERR,
  PARSE_ERR,
  NO_MAGIC_NUM,
  WRONG_DEPTH,
  MALLOC_ERR,
  OUT_OF_RANGE_ERR,
  PIXEL_OOR
};
#endif

// Return value:
//   0 if parsing succeeded
//   1 otherwise
// The function prints appropriate error messages, all main needs to do is to
// exit on failure
extern int parse_file(FILE *fp, struct img_st *dest);

// Return value:
//   0 if completed without errors
//   1 otherwise
extern int write_img(FILE *fp, size_t width, size_t height, const uint8_t *img);

// This function needs to be exposed so that the tests can check for a specific
// error
enum parse_err parse_file_h(FILE *fp, struct img_st *dest);
