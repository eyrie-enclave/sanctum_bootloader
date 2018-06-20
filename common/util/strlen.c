#include <string.h>
#include <stdint.h>
#include <ctype.h>

size_t strlen(const char *s)
{
  const char *p = s;
  while (*p)
    p++;
  return p - s;
}

