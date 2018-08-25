#include "params.h"
#include "gf.h"

#include <stdio.h>

gf eval(gf *f, gf a)
{
	int i;
	gf r;
	
	r = f[ SYS_T ];

	for (i = SYS_T-1; i >= 0; i--)
	{
		r = gf_mul(r, a);
		r = gf_add(r, f[i]);
	}

	return r;
}

void root(gf *out, gf *f, gf *L)
{
	int i; 

	for (i = 0; i < SYS_N; i++)
		out[i] = eval(f, L[i]);
}

