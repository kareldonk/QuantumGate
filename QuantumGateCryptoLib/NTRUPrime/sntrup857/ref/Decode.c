#include "uint16.h"
#include "uint32.h"
#include "Decode.h"

#include <malloc.h>

extern void Decode(uint16 *out,const unsigned char *S,const uint16 *M,long long len)
{
  if (len == 1) {
    if (M[0] == 1)
      *out = 0;
    else if (M[0] <= 256)
      *out = uint32_mod_uint14(S[0],M[0]);
    else
      *out = uint32_mod_uint14(S[0]+(((uint16)S[1])<<8),M[0]);
  }
  if (len > 1) {
    uint16* R2 = (uint16*)_malloca((size_t)((len+1)/2)*sizeof(uint16));
    uint16* M2 = (uint16*)_malloca((size_t)((len+1)/2)*sizeof(uint16));
    uint16* bottomr = (uint16*)_malloca((size_t)(len/2)*sizeof(uint16));
    uint32* bottomt = (uint32*)_malloca((size_t)(len/2)*sizeof(uint32));
    long long i;
    for (i = 0;i < len-1;i += 2) {
      uint32 m = M[i]*(uint32) M[i+1];
      if (m > 256*16383) {
        bottomt[i/2] = 256*256;
        bottomr[i/2] = S[0]+256*S[1];
        S += 2;
        M2[i/2] = (((m+255)>>8)+255)>>8;
      } else if (m >= 16384) {
        bottomt[i/2] = 256;
        bottomr[i/2] = S[0];
        S += 1;
        M2[i/2] = (m+255)>>8;
      } else {
        bottomt[i/2] = 1;
        bottomr[i/2] = 0;
        M2[i/2] = m;
      }
    }
    if (i < len)
      M2[i/2] = M[i];
    Decode(R2,S,M2,(len+1)/2);
    for (i = 0;i < len-1;i += 2) {
      uint32 r = bottomr[i/2];
      uint32 r1;
      uint16 r0;
      r += bottomt[i/2]*R2[i/2];
      uint32_divmod_uint14(&r1,&r0,r,M[i]);
      r1 = uint32_mod_uint14(r1,M[i+1]); /* only needed for invalid inputs */
      *out++ = r0;
      *out++ = r1;
    }
    if (i < len)
      *out++ = R2[i/2];
    _freea(R2);
    _freea(M2);
    _freea(bottomr);
    _freea(bottomt);
  }
}
