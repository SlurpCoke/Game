#ifndef __COLOR_H__
#define __COLOR_H__

#include <stdbool.h>

/**
 * A color represented by RGB values.
 * All values must be in the range [0, 1].
 */
typedef struct color {
  double red;
  double green;
  double blue;
} color_t;

/**
 * Randomly generate rgb values for color of body.
 *
 * @return a color object with randomly generated rgb values
 */
color_t color_get_random();

/**
 * Returns whether two colors are equal.
 *
 * @param c1 the first color
 * @param c2 the second color
 * @return a boolean representing whether the two colors are equal
 */
bool color_is_equal(color_t c1, color_t c2);

#endif // #ifndef __COLOR_H__
