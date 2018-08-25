#include "operations.h"

#include "crypto_hash.h"

#include "params.h"
#include "sk_gen.h"
#include "pk_gen.h"
#include "encrypt.h"
#include "decrypt.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

int crypto_kem_enc(
       unsigned char *c,
       unsigned char *key,
       const unsigned char *pk
)
{
	unsigned char two_e[ 1 + SYS_N/8 ] = {2};
	unsigned char *e = two_e + 1;
	unsigned char one_ec[ 1 + SYS_N/8 + (SYND_BYTES + 32) ] = {1};

	//

	encrypt(c, pk, e);

	if (crypto_hash_32b(c + SYND_BYTES, two_e, sizeof(two_e)) == -1) return -1;

	memcpy(one_ec + 1,e,SYS_N/8);
	memcpy(one_ec + 1 + SYS_N/8,c,SYND_BYTES + 32);

	if (crypto_hash_32b(key, one_ec, sizeof one_ec) == -1) return -1;

	return 0;
}

int crypto_kem_dec(
       unsigned char *key,
       const unsigned char *c,
       const unsigned char *sk
)
{
	int i;

	unsigned char ret_confirm = 0;
	unsigned char ret_decrypt = 0;

	uint16_t m;

	unsigned char conf[32];
	unsigned char two_e[ 1 + SYS_N/8 ] = {2};
	unsigned char *e = two_e + 1;
	unsigned char preimage[ 1 + SYS_N/8 + (SYND_BYTES + 32) ];

	//

	ret_decrypt = decrypt(e, sk + SYS_N/8, c);

	if (crypto_hash_32b(conf, two_e, sizeof(two_e)) == -1) return -1;

	for (i = 0; i < 32; i++)
		ret_confirm |= conf[i] ^ c[SYND_BYTES + i];

	m = ret_decrypt | ret_confirm;
	m -= 1;
	m >>= 8;

	{
		unsigned char *x = preimage;
		*x++ = (~m & 0) |  (m & 1);
		for (i = 0; i < SYS_N/8; i++) 
			*x++ = (~m & sk[i]) | (m & e[i]);
		for (i = 0; i < SYND_BYTES + 32; i++) 
			*x++ = c[i];
	}

	if (crypto_hash_32b(key, preimage, sizeof(preimage)) == -1) return -1;

	return 0;
}

int crypto_kem_keypair
(
       unsigned char *pk,
       unsigned char *sk 
)
{
	int matmem_bytes = (GFBITS * SYS_T) * (SYS_N / 8) * sizeof(unsigned char);
	int gfmem_bytes = ((SYS_T + 1) + SYS_N + SYS_N) * sizeof(gf);

	unsigned char * memory = (unsigned char*)malloc(matmem_bytes + gfmem_bytes);
	if (memory == NULL) return -1;

	unsigned char* matmem = memory;
	gf* gfmem = (gf*)(memory + matmem_bytes);

	while (1)
	{
		if (sk_gen(sk) == 0)
		{
			if (pk_gen(pk, sk + SYS_N / 8, matmem, gfmem) == 0)
				break;
		}
	}
	
	free(memory);

	return 0;
}

