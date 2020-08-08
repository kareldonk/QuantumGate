#include <string.h>
#include <openssl/evp.h>
#include <malloc.h>
#include "aes256ctr.h"

static int aes256ctr_xor(
  unsigned char *out,
  const unsigned char *in,
  unsigned long long inlen,
  const unsigned char *n,
  const unsigned char *k
)
{
  EVP_CIPHER_CTX *x;
  int ok;
  int outl = 0;

  x = EVP_CIPHER_CTX_new();
  if (!x) return -111;

  ok = EVP_EncryptInit_ex(x,EVP_aes_256_ctr(),0,k,n);
  if (ok == 1) ok = EVP_CIPHER_CTX_set_padding(x, 0);
  if (ok == 1) ok = EVP_EncryptUpdate(x, out, &outl, in, (int)inlen);
  if (ok == 1) ok = EVP_EncryptFinal_ex(x, out, &outl);

  EVP_CIPHER_CTX_free(x);
  return ok == 1 ? 0 : -111;
}

int aes256ctr(
  unsigned char *out,
  unsigned long long outlen,
  const unsigned char *n,
  const unsigned char *k
)
{
  unsigned char* temp = (unsigned char*)_malloca((size_t)outlen);
  memset(temp, 0, (size_t)outlen);
  int ret = aes256ctr_xor(out, temp, outlen, n, k);
  _freea(temp);
  return ret;
}
