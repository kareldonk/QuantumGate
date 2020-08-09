#ifndef OPERATIONS_VEC_H
#define OPERATIONS_VEC_H

#ifdef __cplusplus
extern "C" {
#endif
int crypto_kem_mceliece8192128_vec_enc(
       unsigned char *c,
       unsigned char *key,
       const unsigned char *pk
);

int crypto_kem_mceliece8192128_vec_dec(
       unsigned char *key,
       const unsigned char *c,
       const unsigned char *sk
);

int crypto_kem_mceliece8192128_vec_keypair
(
       unsigned char *pk,
       unsigned char *sk 
);
#ifdef __cplusplus
}
#endif

#endif

