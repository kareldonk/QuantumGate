#include "uint32.h"

#pragma warning (disable: 4146)

static void minmax(uint32 *x,uint32 *y)
{
  uint32 xi = *x;
  uint32 yi = *y;
  uint32 xy = xi ^ yi;
  uint32 c = yi - xi;
  c ^= xy & (c ^ yi ^ 0x80000000);
  c >>= 31;
  c = -c;
  c &= xy;
  *x = xi ^ c;
  *y = yi ^ c;
}

void uint32_sort(uint32 *x,int n)
{
  int top,p,q,i;

  if (n < 2) return;
  top = 1;
  while (top < n - top) top += top;

  for (p = top;p > 0;p >>= 1) {
    for (i = 0;i < n - p;++i)
      if (!(i & p))
        minmax(x + i,x + i + p);
    for (q = top;q > p;q >>= 1)
      for (i = 0;i < n - q;++i)
        if (!(i & p))
          minmax(x + i + p,x + i + q);
  }
}
