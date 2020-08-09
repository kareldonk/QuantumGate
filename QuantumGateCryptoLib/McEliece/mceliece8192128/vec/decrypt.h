/*
  This file is for Nieddereiter decryption
*/

#ifndef DECRYPT_H
#define DECRYPT_H
#define decrypt crypto_kem_mceliece8192128_vec_decrypt

int decrypt(unsigned char *, const unsigned char *, const unsigned char *);

#endif

