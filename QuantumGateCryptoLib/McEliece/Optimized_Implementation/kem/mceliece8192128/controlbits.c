#include "controlbits.h"

#include "randombytes.h"
#include "params.h"

#include <stdint.h>
#include <stdlib.h>
#include <cassert>

typedef char bit;

static bit is_smaller(uint32_t a, uint32_t b)
{
  uint32_t ret = 0;

  ret = a - b;
  ret >>= 31;

  return ret;
}

static bit is_smaller_64b(uint64_t a, uint64_t b)
{
  uint64_t ret = 0;

  a >>= 1; 
  b >>= 1;
  ret = a - b;
  ret >>= 63;

  return (bit)ret;
}

static void cswap(uint32_t *x,uint32_t *y,bit swap)
{
  uint32_t m;
  uint32_t d;

  m = swap; 
  m = 0 - m;

  d = (*x ^ *y);
  d &= m;
  *x ^= d;
  *y ^= d;
}

static void cswap_64b(uint64_t *x,uint64_t *y,bit swap)
{
  uint64_t m;
  uint64_t d;
  uint64_t z = 0;

  z -= swap ;
  m = z;

  d = (*x ^ *y);
  d &= m;
  *x ^= d;
  *y ^= d;
}

/* output x = min(input x,input y) */
/* output y = max(input x,input y) */

static void minmax(uint32_t *x, uint32_t *y)
{
  bit m;

  m = is_smaller(*y, *x);
  cswap(x, y, m);
}

static void minmax_64b(uint64_t *x, uint64_t *y)
{
  bit m;

  m = is_smaller_64b(*y, *x);
  cswap_64b(x, y, m);
}

/* merge first half of x[0],x[step],...,x[(2*n-1)*step] with second half */
/* requires n to be a power of 2 */

static void merge(int n,uint32_t *x,int step)
{
  int i;
  if (n == 1)
    minmax(&x[0],&x[step]);
  else {
    merge(n / 2,x,step * 2);
    merge(n / 2,x + step,step * 2);
    for (i = 1;i < 2*n-1;i += 2)
      minmax(&x[i * step],&x[(i + 1) * step]);
  }
}

static void merge_64b(int n,uint64_t *x,int step)
{
  int i;
  if (n == 1)
    minmax_64b(&x[0],&x[step]);
  else {
    merge_64b(n / 2,x,step * 2);
    merge_64b(n / 2,x + step,step * 2);
    for (i = 1;i < 2*n-1;i += 2)
      minmax_64b(&x[i * step],&x[(i + 1) * step]);
  }
}

/* sort x[0],x[1],...,x[n-1] in place */
/* requires n to be a power of 2 */

static void sort(int n, uint32_t *x)
{
  if (n <= 1) return;
  sort(n/2,x);
  sort(n/2,x + n/2);
  merge(n/2,x,1);
}

static void sort_64b(int n, uint64_t *x)
{
  if (n <= 1) return;
  sort_64b(n/2,x);
  sort_64b(n/2,x + n/2);
  merge_64b(n/2,x,1);
}

/* y[pi[i]] = x[i] */
/* requires n = 2^w */
/* requires pi to be a permutation */
static void composeinv(int n,uint32_t *y,uint32_t *x,uint32_t *pi) // NC
{
#define cn  16384
  int i;
  uint32_t t[cn];
  
  assert(n <= cn);

  for (i = 0;i < n;++i) 
    t[i] = x[i] | (pi[i] << 16);

  sort(n,t);

  for (i = 0;i < n;++i)
    y[i] = t[i] & 0xFFFF;
}

/* ip[i] = j iff pi[i] = j */
/* requires n = 2^w */
/* requires pi to be a permutation */
static void invert(int n,uint32_t *ip,uint32_t *pi)
{
  int i;

  for (i = 0;i < n;i++)
    ip[i] = i;

  composeinv(n,ip,ip,pi);
}


static void flow(int w, uint32_t *x, uint32_t *y, const int t)
{
  bit m0;
  bit m1;

  uint32_t b;
  uint32_t y_copy = *y;

  m0 = is_smaller(*y & ((1<<w)-1), *x & ((1<<w)-1));
  m1 = is_smaller(0, t);

  cswap(x, &y_copy, m0);
  b = m0 & m1;
  *x ^= b << w;
}

/* input: permutation pi */
/* output: (2w-1)n/2 (or 0 if n==1) control bits c[0],c[step],c[2*step],... */
/* requires n = 2^w */
static int controlbitsfrompermutation(int w, int n, int step, int off, unsigned char *c, uint32_t *pi)
{
  if (w == 1) c[off / 8] |= (pi[0] & 1) << (off % 8);
  if (w <= 1) return 0;

  int i;
  int j;
  int k;
  int t;

  /*
  uint32_t ip[n];
  uint32_t I[2 * n];
  uint32_t P[2 * n];
  uint32_t PI[2 * n];
  uint32_t T[2 * n];
  uint32_t piflip[n];
  uint32_t subpi[2][n / 2];
  */

  // Above arrays allocated on the heap instead to
  // avoid exhausting the stack
  uint32_t* memory = (uint32_t*)malloc((n + (4 * (2 * n)) + n + (2 * (n / 2))) * sizeof(uint32_t));
  if (memory == NULL) return -1;

  uint32_t* ip = memory;
  uint32_t* I = memory + n;
  uint32_t* P = memory + n + (2 * n);
  uint32_t* PI = memory + n + (2 * (2 * n));
  uint32_t* T = memory + n + (3 * (2 * n));
  uint32_t* piflip = memory + n + (4 * (2 * n));
  
  uint32_t* subpi[2];
  subpi[0] = memory + n + (4 * (2 * n) + n);
  subpi[1] = memory + n + (4 * (2 * n) + n + (n / 2));

  invert(n,ip,pi);

  for (i = 0;i < n;++i) 
  {
    I[i] = ip[i] | (1 << w);
    I[n + i] = pi[i];
  }

  for (i = 0;i < 2 * n;++i)
      P[i] = (i >> w) + (i & ((1<<w)-2)) + ((i & 1) << w);

  for (t = 0;t < w;++t) 
  {
    composeinv(2 * n,PI,P,I);

    for (i = 0;i < 2 * n;++i)
      flow(w,&P[i],&PI[i],t);

    for (i = 0;i < 2 * n;++i)
	T[i] = I[i ^ 1];

    composeinv(2 * n,I,I,T);

    for (i = 0;i < 2 * n;++i)
	T[i] = P[i ^ 1];

    for (i = 0;i < 2 * n;++i)
      flow(w,&P[i],&T[i],1);
  }

  for (i = 0;i < n;++i)
    for (j = 0;j < w;++j)
      piflip[i] = pi[i];

  for (i = 0;i < n / 2;++i) c[ (off + i * step)/8 ] |= ((P[i * 2] >> w) & 1) << ((off + i * step)%8);
  for (i = 0;i < n / 2;++i) c[ (off + ((w-1)*n + i) * step)/8 ] |= ((P[n + i * 2] >> w) & 1) << ((off + ((w-1)*n + i) * step)%8);

  for (i = 0;i < n / 2;++i)
    cswap(&piflip[i * 2], &piflip[i * 2 + 1], (P[n + i * 2] >> w) & 1);

  for (k = 0;k < 2;++k)
    for (i = 0;i < n / 2;++i)
        subpi[k][i] = piflip[i * 2 + k] >> 1;

  for (k = 0;k < 2;++k)
    controlbitsfrompermutation(w - 1, n / 2, step * 2, off + step * (n/2 + k), c, subpi[k]);

  free(memory);

  return 0;
}

static void perm_gen(uint32_t *out)
{
	int i, j, rep;

	uint64_t list[ 1 << GFBITS ];
	
	while(1)
	{
		randombytes((unsigned char *) list, sizeof(list));

		for (i = 0; i < (1 << GFBITS); i++)
		{
			list[i] <<= GFBITS;
			list[i] |= i;
		}

		rep = 0;
		for (i = 1; i < (1 << GFBITS); i++)
			for (j = 0; j < i; j++)
				if ((list[j] >> GFBITS) == (list[i] >> GFBITS))
					rep = 1;

		if (rep == 0) break;
	}

	sort_64b(1 << GFBITS, list);

	for (i = 0; i < (1 << GFBITS); i++)
		out[i] = list[i] & GFMASK;
}

int controlbits(unsigned char * out)
{
	int i;
	uint32_t pi[ (1 << GFBITS) ];
	unsigned char c[ (2*GFBITS - 1) * (1 << GFBITS) / 16 ];

	perm_gen(pi);

	for (i = 0; i < sizeof(c); i++)
		c[i] = 0;

	if (controlbitsfrompermutation(GFBITS, (1 << GFBITS), 1, 0, c, pi) == -1) return -1;

	for (i = 0; i < sizeof(c); i++)
		out[i] = c[i];

	return 0;
}

