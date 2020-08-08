#include <stddef.h>
#include <openssl/sha.h>
#include "sha512.h"

int sha512(unsigned char *out,const unsigned char *in,unsigned long long inlen)
{
  SHA512(in, (size_t)inlen,out);
  return 0;
}
