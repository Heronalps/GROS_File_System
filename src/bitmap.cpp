/**
 * bitmap.cpp
 */

#include "bitmap.hpp"

/**
 * Returns a new instance of a Bitmap with `size` addressable elements.
 *  Each element in the bitmap is represented as a bit, indexed by the
 *  BIT offset. If the bit is set (i.e. 1), then that element is "in use".
 *  Otherwise, the element is free.
 *
 *  @param int size    The number of addressable elements
 *  @param char *buf   A pointer to array of bytes containing the data structure
 */
Bitmap* init_bitmap(int size, char *buf) {
  Bitmap *bm = new Bitmap;
  bm->size = size;
  bm->buf = buf;
  return bm;
}

/**
 * Returns the index of the first bit which is set to 0 (i.e. unused).
 *  If there are no unset bits, then this returns -1.
 *
 * @param Bitmap *bm   The bitmap to check
 */
int first_unset_bit(Bitmap *bm) {
  int i, isset = 1;
  int byte, offset;
  for (i=0; i<bm->size && isset == 1; i++) {
    byte = i / 8;
    offset = i % 8;
    isset = (bm->buf[byte] & (1 << offset)) > 0;
  }
  if (i >= bm->size) {
    return -1;
  } else {
    return i-1;
  }
}

/**
 * Returns whether the bit at index `index` is set (1) or not (0). 
 *  If `index` is out of bounds (0 < index < bm->size), then this returns 0.
 *
 * @param Bitmap *bm   The bitmap to check
 * @param int index    The index to check in the bitmap
 */
short is_bit_set(Bitmap *bm, int index) {
  if (index < 0 || index >= bm->size) {
    return 1;
  }
  int byte = index / 8;
  int offset = index % 8;
  return (bm->buf[byte] & (1 << offset)) > 0;
}

/**
 * Sets the bit at index `index` to 1, indicating that that index is now in use.
 *  If `index` is out of bounds (0 < index < bm->size), then this returns -1.
 *  Otherwise, this returns the index passed in.
 *
 * @param Bitmap *bm   The bitmap to check
 * @param int index    The index to check in the bitmap
 */
int set_bit(Bitmap *bm, int index) {
  if (index < 0 || index >= bm->size) {
    return -1;
  }
  int byte = index / 8;
  int offset = index % 8;
  bm->buf[byte] = bm->buf[byte] | (1 << offset);
  return index;
}

/**
 * Sets the bit at index `index` to 0, indicating that that index is no longer in use.
 *  If `index` is out of bounds (0 < index < bm->size), then this returns -1.
 *  Otherwise, this returns the index passed in.
 *
 * @param Bitmap *bm   The bitmap to check
 * @param int index    The index to check in the bitmap
 */
int unset_bit(Bitmap *bm, int index) {
  if (index < 0 || index >= bm->size) {
    return -1;
  }
  int byte = index / 8;
  int offset = index % 8;
  bm->buf[byte] = bm->buf[byte] & ~(1 << offset);
  return index;
}
