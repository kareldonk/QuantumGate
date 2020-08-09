// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "mceliece8192128.h"
#include "ref\operations.h"
#include "vec\operations.h"

int crypto_kem_mceliece8192128_enc(unsigned char* c, unsigned char* key, const unsigned char* pk)
{
#ifdef MCELIECE_USE_VEC
	return crypto_kem_mceliece8192128_vec_enc(c, key, pk);
#else
	return crypto_kem_mceliece8192128_ref_enc(c, key, pk);
#endif
}

int crypto_kem_mceliece8192128_dec(unsigned char* key, const unsigned char* c, const unsigned char* sk)
{
#ifdef MCELIECE_USE_VEC
	return crypto_kem_mceliece8192128_vec_dec(key, c, sk);
#else
	return crypto_kem_mceliece8192128_ref_dec(key, c, sk);
#endif
}

int crypto_kem_mceliece8192128_keypair(unsigned char* pk, unsigned char* sk)
{
#ifdef MCELIECE_USE_VEC
	return crypto_kem_mceliece8192128_vec_keypair(pk, sk);
#else
	return crypto_kem_mceliece8192128_ref_keypair(pk, sk);
#endif
}
