#ifndef Encode_H
#define Encode_H

/* Encode(s,R,M,len) */
/* assumes 0 <= R[i] < M[i] < 16384 */
extern void Encode(unsigned char *,const uint16 *,const uint16 *,long long);

#endif
