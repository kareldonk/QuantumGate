/* 
  This file is for functions required for generating the control bits of the Benes network w.r.t. a random permutation
  see the Lev-Pippenger-Valiant paper https://www.computer.org/csdl/trans/tc/1981/02/06312171.pdf
*/

#ifndef CONTROLBITS_H
#define CONTROLBITS_H

#include <stdint.h>

void sort_63b(int, uint64_t *);
int controlbits(unsigned char *, uint32_t *);

#endif

