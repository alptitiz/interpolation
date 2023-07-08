#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_parsing.h"
#include "util.h"

// Arbitrary choice, though making it too short will make parsing slow.
// The standard for plain-text PPM recommends that lines be no longer than 70
// characters; we add two bytes for linebreak and the null byte. This way, lines
// that follow this recommendation will be read completely with only one
// fgets().
#define FGETS_LENGTH 72

enum parse_type { P3_FILE, P6_FILE };

// Struct to keep track of line-related information we need during parsing.
//   lineptr: pointer to beginning of line (will be used to call free())
//   linepos: pointer to current position in the line (where we start parsing)
struct line_info_st {
  char *lineptr;
  const char *linepos;
};

// We can't use strlen() because fgets() might have read null bytes from the P6
// image raster.
size_t ln_length(char *ln, size_t max_length) {
  for (size_t len = 0; len < max_length; len++) {
    if (ln[len] == '\n')
      return len;
  }
  return max_length;
}

// Purpose: Read an integer correctly even if fgets didn't read the whole line
// Note:    ln_info->lineptr must be at least FGETS_LENGTH bytes big
// If valid result, sets ln_info->linepos to the last character that was parsed
// and res to the parsed number.
enum parse_err read_number(FILE *fp, struct line_info_st *ln, size_t *val,
                           bool allow_comments) {
  const char *endptr, *numptr;
  *val = 0;
  size_t val_cur, val_old;

start:
  errno = 0;
  val_cur = strtosizet(ln->linepos, &endptr, &numptr);
  if (errno == ERANGE)
    return OUT_OF_RANGE_ERR;


 if (*val != 0 && ln->linepos != numptr) {
    endptr = ln->linepos; // Don't update endptr, because the whitespace
                          // "belongs" to the next number, which we haven't
                          // parsed yet.
    goto parse_end;
  }


  // In any case, if we found digits, we'll need to add them to the number
  if (numptr) {
    errno = 0;
    if (*val != 0)
      *val *= int_pow(10, endptr - numptr);
    if (errno == ERANGE)
      return OUT_OF_RANGE_ERR;
    val_old = *val;
    *val += val_cur;
    if (*val < val_old)
      return OUT_OF_RANGE_ERR;
  }

  // Now we look at the end pointer to decide what to do next.
  if (*endptr == 0) {

    // We must distinguish two cases:
    //   1. There is a null byte in the header. Null bytes are not whitespace
    //      and thus not allowed. Return a PARSE_ERR.
    //   2. The line is too big for the buffer and we need to read the next
    //      FGETS_LENGTH characters to continue parsing.
    //      If reading the next FGETS_LENGTH characters fails, this can mean
    //      three things:
    //       2.1. The file has ended, we got the last number, return PARSE_OK.
    //       2.2. The file has ended, we got no number, return READ_ERR so this
    //       can get handled as "EOF before parsing could finish".
    //       2.2. There was a read error, return READ_ERR.
    if (strlen(ln->lineptr) < FGETS_LENGTH - 1 &&
        ln->lineptr[strlen(ln->lineptr) > 0 ? strlen(ln->lineptr) - 1 : 0] !=
            '\n' &&
        !feof(fp))
      return PARSE_ERR;

    if (fgets(ln->lineptr, FGETS_LENGTH, fp) == NULL) {
      if (feof(fp) && numptr)
        goto parse_end;
      else
        return READ_ERR;
    }
    ln->linepos = ln->lineptr;
    goto start;

  } else if (*endptr == '#' && allow_comments) {
    // Encountered a comment, skip over it.
    // Consider that the PPM specification allows comments to appear even in the
    // middle of a number, so even if we encountered digits before, we skip over
    // the comment and continue parsing.

    while (strlen(ln->lineptr) == FGETS_LENGTH - 1 &&
           ln->lineptr[FGETS_LENGTH - 2] != '\n') {
      if (fgets(ln->lineptr, FGETS_LENGTH, fp) == NULL)
        return READ_ERR;
    }
    if (fgets(ln->lineptr, FGETS_LENGTH, fp) == NULL)
      return READ_ERR;
    ln->linepos = ln->lineptr;
    goto start;

  } else if (isspace(*endptr)) {
    // We ended at a whitespace, which means that we have read a valid number
    // and can return it. We know for sure that we encountered a number, because
    // if the entire string is whitespace, the result is *endptr == 0; note that
    // '\n' and '\r' are considered whitespace.

  } else {
    // Parsing stopped at an invalid character, the file is not a valid PPM
    return PARSE_ERR;
  }

parse_end:
  ln->linepos = endptr;
  return PARSE_OK;
}

enum parse_err parse_file_header(FILE *fp, struct img_st *dest,
                                 struct line_info_st *ln) {
  enum parse_err res = PARSE_OK;
  size_t vals[3];

  for (int i = 0; i < 3; i++) {
    res = read_number(fp, ln, &vals[i], true);
    if (res != PARSE_OK)
      return res;
  }

  if (vals[2] != 255) {
    return WRONG_DEPTH;
  }
  dest->width = vals[0];
  dest->height = vals[1];

  return PARSE_OK;
}

enum parse_err parse_file_p3(FILE *fp, struct img_st *dest,
                             struct line_info_st *lastln) {
  enum parse_err res;
  size_t tmp;

  for (size_t read_cur = 0; read_cur < dest->width * dest->height * 3;
       read_cur++) {
    res = read_number(fp, lastln, &tmp, false);
    if (res == OUT_OF_RANGE_ERR)
      return PIXEL_OOR;
    if (res != PARSE_OK)
      return res;
    if (res > 255)
      return PIXEL_OOR;
    dest->img[read_cur] = tmp;
  }

  return PARSE_OK;
}

enum parse_err parse_file_p6(FILE *fp, struct img_st *dest,
                             struct line_info_st *lastln) {
  // DO NOT USE fgets() FOR READING IMAGE RASTER!
  // If our image anywhere contains the byte 10 (ASCII for '\n'), it would not
  // get the full image. Use fread() instead, it doesn't care about newlines.

  // If the whitespace that separates colour depth from raster was not a '\n',
  // fgets() has read more than it should have. Copy the remaining contents of
  // the buffer to dest->img.
  size_t imgbuf_size = dest->width * dest->height * 3;
  size_t lastln_size = ln_length(lastln->lineptr, FGETS_LENGTH) -
                       (lastln->linepos - lastln->lineptr);

  if (lastln_size >= imgbuf_size) {
    memcpy(dest->img, lastln->linepos + 1, imgbuf_size);
    return PARSE_OK;
  }

  memcpy(dest->img, lastln->linepos + 1, lastln_size);
  imgbuf_size -= lastln_size;

  size_t r = fread(dest->img, 1, imgbuf_size, fp);
  if (r < imgbuf_size) {
    return READ_ERR;
  }
  return PARSE_OK;
}

enum parse_err parse_file_h(FILE *fp, struct img_st *dest) {
  // Before anything else, check for magic number
  enum parse_type ptype;
  enum parse_err res = PARSE_OK;
  dest->img = NULL;
  int c = fgetc(fp);
  if (c == EOF)
    return READ_ERR;
  if (c != 'P')
    return NO_MAGIC_NUM;
  c = fgetc(fp);
  switch (c) {
  case '3':
    ptype = P3_FILE;
    break;
  case '6':
    ptype = P6_FILE;
    break;
  case EOF:
    return READ_ERR;
  default:
    return NO_MAGIC_NUM;
  }

  // Allocate and initialize an appropriate line buffer, and start
  // parsing header
  struct line_info_st lastln;
  lastln.lineptr = malloc(FGETS_LENGTH * sizeof(char));
  lastln.linepos = lastln.lineptr;
  if (fgets(lastln.lineptr, FGETS_LENGTH, fp) == NULL) {
    res = READ_ERR;
    goto cleanup;
  }

  res = parse_file_header(fp, dest, &lastln);
  if (res != PARSE_OK)
    goto cleanup;

  // Now, let's allocate a buffer to fit our image into.
  errno = 0;
  size_t imgbuf_size = input_imgsize(dest->width, dest->height);
  if (errno == ERANGE) {
    res = MALLOC_ERR;
    goto cleanup;
  }
  if (dest->width * dest->height == 0) {
    dest->img = NULL;
    goto cleanup;
  }
  dest->img = malloc(imgbuf_size * sizeof(char));
  if (!dest->img) {
    res = MALLOC_ERR;
    goto cleanup;
  }

  switch (ptype) {
  case P3_FILE:
    res = parse_file_p3(fp, dest, &lastln);
    break;
  case P6_FILE:
    res = parse_file_p6(fp, dest, &lastln);
    break;
  }

cleanup:
  if (res != PARSE_OK && dest->img)
    free(dest->img);
  if (lastln.lineptr)
    free(lastln.lineptr);
  return res;
}

// Wrapper around parse_file_h() that does error handling
int parse_file(FILE *fp, struct img_st *dest) {
  enum parse_err res = parse_file_h(fp, dest);
  if (res != PARSE_OK)
    dest->img =
        NULL; // It was already freed, make that clear by setting it to NULL
  switch (res) {
  case PARSE_OK:
    return 0;
  case READ_ERR:
    if (feof(fp))
      fprintf(stderr, "EOF reached before parsing could finish.\n");
    // We assume that fgets() sets errno, as specified by POSIX:
    // https://pubs.opengroup.org/onlinepubs/9699919799/functions/fgets.html
    else
      perror("Error reading input file");
    return 1;
  case PARSE_ERR:
    fprintf(stderr, "Error parsing input file.\n");
    return 1;
  case NO_MAGIC_NUM:
    fprintf(stderr, "Missing the magic number P6.\n");
    return 1;
  case WRONG_DEPTH:
    fprintf(stderr, "Colour depth not 24 bit. This program only supports depth "
                    "of 24 bit (maxval 255).\n");
    return 1;
  case MALLOC_ERR:
    fprintf(stderr, "Failed to allocate sufficient memory.\n");
    return 1;
  case OUT_OF_RANGE_ERR:
    fprintf(stderr,
            "Error: width, height or maxval out of range for size_t.\n");
    return 1;
  case PIXEL_OOR:
    fprintf(stderr, "Error: plain-text pixel value out of range.\n");
    return 1;
  default:
    fprintf(stderr, "Unknown error.\n"); // Shouldn't happen, just here to
                                         // remove compiler warning
    return 1;
  }
}

int write_img(FILE *fp, size_t width, size_t height, const uint8_t *img) {
  if (fprintf(fp, "P6\n") < 0)
    return 1;
  if (fprintf(fp, "%zu %zu\n", width, height) < 0)
    return 1;
  if (fprintf(fp, "255\n") < 0)
    return 1;
  size_t img_size = 3 * width * height;
  if (img_size > 0 && fwrite(img, 1, img_size, fp) < img_size)
    return 1;
  return 0;
}
