#ifndef OPERATIONS_H
#define OPERATIONS_H

int crypto_kem_mceliece8192128_ref_enc(
       unsigned char *c,
       unsigned char *key,
       const unsigned char *pk
);

int crypto_kem_mceliece8192128_ref_dec(
       unsigned char *key,
       const unsigned char *c,
       const unsigned char *sk
);

int crypto_kem_mceliece8192128_ref_keypair
(
       unsigned char *pk,
       unsigned char *sk 
);

#endif

