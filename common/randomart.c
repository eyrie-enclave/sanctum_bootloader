
#include"randomart.h"
#include <string.h>

// Adopted from https://gist.github.com/nirenjan/4450419

#define XLIM 17
#define YLIM 9

const char symbols[] = {
    ' ', '.', 'o', '+',
    '=', '*', 'B', 'O',
    'X', '@', '%', '&',
    '#', '/', '^', 'S', 'E'
};

uint8_t new_position(uint8_t *pos, uint8_t direction)
{
    uint8_t newpos;
    uint8_t upd = 1;
    int8_t x0;
    int8_t y0;
    int8_t x1;
    int8_t y1;

    x0 = *pos % XLIM;
    y0 = *pos / XLIM;

    switch (direction) {
        case 0: // NW
            x1 = x0 - 1;
            y1 = y0 - 1;
            break;
        case 1: // NE
            x1 = x0 + 1;
            y1 = y0 - 1;
            break;
        case 2: // SW
            x1 = x0 - 1;
            y1 = y0 + 1;
            break;
        case 3: // SE
            x1 = x0 + 1;
            y1 = y0 + 1;
            break;
        default: // Should never happen
            x1 = x0;
            y1 = y0;
            break;
    }

    // Limit the range of x1 & y1
    if (x1 < 0) {
        x1 = 0;
    } else if (x1 >= XLIM) {
        x1 = XLIM - 1;
    }

    if (y1 < 0) {
        y1 = 0;
    } else if (y1 >= YLIM) {
        y1 = YLIM - 1;
    }

    newpos = y1 * XLIM + x1;

    if (newpos == *pos) {
        upd = 0;
    } else {
        *pos = newpos;
    }

    return upd;
}

void drunken_walk(uint8_t* input, size_t len, uint16_t array[XLIM*YLIM]) {
    uint8_t pos;
    uint8_t upd;
    uint8_t temp;

    pos = 76;
    for (unsigned int idx = 0; idx < len; idx++) {
        temp = input[idx];

        for (unsigned int i = 0; i < 4; i++) {
            upd = new_position(&pos, temp & 3);
            if (upd) {
                array[pos]++;
            }
            temp >>= 2;
        }
    }

    array[pos] = 16; // End
    array[76] = 15; // Start
}

void randomart(uint8_t* input, size_t len, char* out_str) {
  // Compute the randomart image from bytes of pk
  unsigned int strptr = 0;
  uint16_t array[XLIM * YLIM] = {0};
  drunken_walk(input, len, array);

  // Print the randomart image to out_str

  //memcpy(out_str, "+--[ED25519 256]--+\n", 20*sizeof(char));
  //out_str += 20;

  for (unsigned int i = 0; i < YLIM; i++) {
    *(out_str+(strptr++)) = '|';
    for (unsigned int j = 0; j < XLIM; j++) {
      *(out_str+(strptr++)) = symbols[ array[j + XLIM * i] ];
    }
    *(out_str+(strptr++)) = '|';
    *(out_str+(strptr++)) = '\n';
  }
  memcpy(out_str+strptr, "+-----------------+\n", 20*sizeof(char));
  strptr+=20;
  *(out_str+strptr) = '\0';
}
