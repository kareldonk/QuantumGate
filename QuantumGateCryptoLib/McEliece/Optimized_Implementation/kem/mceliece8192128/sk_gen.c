#include "sk_gen.h"

#include "randombytes.h"
#include "controlbits.h"
#include "params.h"
#include "util.h"
#include "gf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int irr_gen(gf *out, gf *f)
{
	int i, j, k, c;

	gf mat[ SYS_T+1 ][ SYS_T ];
	gf mask, inv, t;

	//

	mat[0][0] = 1;

	for (i = 1; i < SYS_T; i++)
		mat[0][i] = 0;

	for (i = 0; i < SYS_T; i++)
		mat[1][i] = f[i];

	for (j = 2; j <= SYS_T; j++)
		GF_mul(mat[j], mat[j-1], f);

	//

	for (j = 0; j < SYS_T; j++)
	{
		for (k = j + 1; k < SYS_T; k++)
		{
			mask = gf_iszero(mat[ j ][ j ]);

			for (c = j; c < SYS_T + 1; c++)
				mat[ c ][ j ] ^= mat[ c ][ k ] & mask;

		}

		if ( mat[ j ][ j ] == 0 ) // return if not systematic
		{
			return -1;
		}

		inv = gf_inv(mat[j][j]);

		for (c = j; c < SYS_T + 1; c++)
			mat[ c ][ j ] = gf_mul(mat[ c ][ j ], inv) ;

		for (k = 0; k < SYS_T; k++)
		{
			if (k != j)
			{
				t = mat[ j ][ k ];

				for (c = j; c < SYS_T + 1; c++)
					mat[ c ][ k ] ^= gf_mul(mat[ c ][ j ], t);
			}
		}
	}

	for (i = 0; i < SYS_T; i++)
		out[i] = mat[ SYS_T ][ i ];

	return 0;
}

int sk_gen(unsigned char *sk)
{
	int i;

	gf g[ SYS_T ]; // irreducible polynomial
	gf a[ SYS_T ]; // random element in GF(2^mt)
	
	while (1)
	{
		randombytes((unsigned char *) a, sizeof(a)); 

		for (i = 0; i < SYS_T; i++) a[i] &= GFMASK;

		if ( irr_gen(g, a) == 0 ) break;
	}

	for (i = 0; i < SYS_T; i++) 
		store2( sk + SYS_N/8 + i*2, g[i] );

	randombytes(sk, SYS_N/8);

	if (controlbits(sk + SYS_N / 8 + IRR_BYTES) == -1) return -1;

	return 0;
}

