/*
  This file is for secret-key generation
*/

#include "sk_gen.h"

#include "..\..\..\Common\randombytes.h"
#include "..\ref\controlbits.h"
#include "..\ref\params.h"
#include "util.h"
#include "gf.h"

/* input: f, element in GF((2^m)^t) */
/* output: out, minimal polynomial of f */
/* return: 0 for success and -1 for failure */
int genpoly_gen(gf *out, gf *f)
{
	int i, j, k, c;

	gf mat[ SYS_T+1 ][ SYS_T ];
	gf mask, inv, t;

	// fill matrix

	mat[0][0] = 1;

	for (i = 1; i < SYS_T; i++)
		mat[0][i] = 0;

	for (i = 0; i < SYS_T; i++)
		mat[1][i] = f[i];

	for (j = 2; j <= SYS_T; j++)
		GF_mul(mat[j], mat[j-1], f);

	// gaussian

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

