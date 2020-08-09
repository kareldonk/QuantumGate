// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#define MCELIECE_USE_VEC

#define crypto_kem_mceliece8192128_PUBLICKEYBYTES 1357824
#define crypto_kem_mceliece8192128_SECRETKEYBYTES 14080
#define crypto_kem_mceliece8192128_CIPHERTEXTBYTES 240
#define crypto_kem_mceliece8192128_BYTES 32

int crypto_kem_mceliece8192128_keypair(unsigned char*, unsigned char*);
int crypto_kem_mceliece8192128_enc(unsigned char*, unsigned char*, const unsigned char*);
int crypto_kem_mceliece8192128_dec(unsigned char*, const unsigned char*, const unsigned char*);

