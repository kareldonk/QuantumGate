/*
  This file is for Niederreiter encryption
*/

#include "encrypt.h"

#include "..\..\..\Common\randombytes.h"
#include "..\ref\params.h"
#include "util.h"

#include <stdint.h>

/* output: e, an error vector of weight t */
static void gen_e(unsigned char *e)
{
	int i, j, eq;

	uint16_t ind[ SYS_T ];
	unsigned char bytes[ sizeof(ind) ];
	uint64_t e_int[ SYS_N/64 ];	
	uint64_t one = 1;	
	uint64_t mask;	
	uint64_t val[ SYS_T ];	

	while (1)
	{
		randombytes(bytes, sizeof(bytes));

		for (i = 0; i < SYS_T; i++)
			ind[i] = load_gf(bytes + i*2);

		// check for repetition

		eq = 0;

		for (i = 1; i < SYS_T; i++) 
			for (j = 0; j < i; j++)
				if (ind[i] == ind[j]) 
					eq = 1;

		if (eq == 0)
			break;
	}

	for (j = 0; j < SYS_T; j++)
		val[j] = one << (ind[j] & 63);

	for (i = 0; i < SYS_N/64; i++) 
	{
		e_int[i] = 0;

		for (j = 0; j < SYS_T; j++)
		{
			mask = i ^ (ind[j] >> 6);
			mask -= 1;
			mask >>= 63;
			mask = -mask;

			e_int[i] |= val[j] & mask;
		}
	}

	for (i = 0; i < SYS_N/64; i++)
		store8(e + i*8, e_int[i]);
}

/* input: public key pk, error vector e */
/* output: syndrome s */
static void syndrome(unsigned char *s, const unsigned char *pk, unsigned char *e)
{
	uint64_t b;

	const uint64_t *pk_ptr; 
	const uint64_t *e_ptr = ((uint64_t *) (e + SYND_BYTES));

	int i, j;

	//

	for (i = 0; i < SYND_BYTES; i++)
		s[i] = e[i];

	for (i = 0; i < PK_NROWS; i++)	
	{
		pk_ptr = ((uint64_t *) (pk + PK_ROW_BYTES * i));
	
		b = 0;
		for (j = 0; j < PK_NCOLS/64; j++)
			b ^= pk_ptr[j] & e_ptr[j];

		b ^= b >> 32;
		b ^= b >> 16;
		b ^= b >> 8;
		b ^= b >> 4;
		b ^= b >> 2;
		b ^= b >> 1;
		b &= 1;

		s[ i/8 ] ^= (b << (i%8));
	}
}

/* input: public key pk */
/* output: error vector e, syndrome s */
void encrypt(unsigned char *s, const unsigned char *pk, unsigned char *e)
{
	gen_e(e);

#ifdef KAT
  {
    int k;
    printf("encrypt e: positions");
    for (k = 0;k < SYS_N;++k)
      if (e[k/8] & (1 << (k&7)))
        printf(" %d",k);
    printf("\n");
  }
#endif

	syndrome(s, pk, e);
}

