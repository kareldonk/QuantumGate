#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "pk_gen.h"
#include "params.h"
#include "benes.h"
#include "root.h"
#include "util.h"

int pk_gen(unsigned char * pk, unsigned char * sk, unsigned char * matmem, gf * gfmem)
{
	int i, j, k;
	int row, c;

	/*
	unsigned char mat[ GFBITS * SYS_T ][ SYS_N/8 ];
	unsigned char mask;
	unsigned char b;

	gf g[ SYS_T+1 ];
	gf L[ SYS_N ];
	gf inv[ SYS_N ];
	*/

	// Above arrays allocated on the heap instead to
	// avoid exhausting the stack
	unsigned char* mat[GFBITS * SYS_T];
	for (int x = 0; x < (GFBITS * SYS_T); x++)
	{
		mat[x] = matmem + (x * (SYS_N / 8));
	}

	unsigned char mask;
	unsigned char b;

	gf* g = gfmem;
	gf* L = gfmem + (SYS_T + 1);
	gf* inv = gfmem + (SYS_T + 1) + SYS_N;

	//

	g[ SYS_T ] = 1;

	for (i = 0; i < SYS_T; i++) { g[i] = load2(sk); g[i] &= GFMASK; sk += 2; }

	support_gen(L, sk);

	root(inv, g, L);
		
	for (i = 0; i < SYS_N; i++)
		inv[i] = gf_inv(inv[i]);

	//

	for (i = 0; i < PK_NROWS; i++)
	for (j = 0; j < SYS_N/8; j++)
		mat[i][j] = 0;

	for (i = 0; i < SYS_T; i++)
	{
		for (j = 0; j < SYS_N; j+=8)
		for (k = 0; k < GFBITS;  k++)
		{
			b  = (inv[j+7] >> k) & 1; b <<= 1;
			b |= (inv[j+6] >> k) & 1; b <<= 1;
			b |= (inv[j+5] >> k) & 1; b <<= 1;
			b |= (inv[j+4] >> k) & 1; b <<= 1;
			b |= (inv[j+3] >> k) & 1; b <<= 1;
			b |= (inv[j+2] >> k) & 1; b <<= 1;
			b |= (inv[j+1] >> k) & 1; b <<= 1;
			b |= (inv[j+0] >> k) & 1;

			mat[ i*GFBITS + k ][ j/8 ] = b;
		}

		for (j = 0; j < SYS_N; j++)
			inv[j] = gf_mul(inv[j], L[j]);

	}

	//

	for (i = 0; i < (GFBITS * SYS_T + 7) / 8; i++)
	for (j = 0; j < 8; j++)
	{
		row = i*8 + j;			

		if (row >= GFBITS * SYS_T)
			break;

		for (k = row + 1; k < GFBITS * SYS_T; k++)
		{
			mask = mat[ row ][ i ] ^ mat[ k ][ i ];
			mask >>= j;
			mask &= 1;
			mask = -mask;

			for (c = 0; c < SYS_N/8; c++)
				mat[ row ][ c ] ^= mat[ k ][ c ] & mask;
		}

		if ( ((mat[ row ][ i ] >> j) & 1) == 0 ) // return if not systematic
		{
			return -1;
		}

		for (k = 0; k < GFBITS * SYS_T; k++)
		{
			if (k != row)
			{
				mask = mat[ k ][ i ] >> j;
				mask &= 1;
				mask = -mask;

				for (c = 0; c < SYS_N/8; c++)
					mat[ k ][ c ] ^= mat[ row ][ c ] & mask;
			}
		}
	}

	for (i = 0; i < PK_NROWS; i++)
		memcpy(pk + i*PK_ROW_BYTES, mat[i] + PK_NROWS/8, PK_ROW_BYTES);

	return 0;
}

