// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common\Random.h"
#include "McEliece\Optimized_Implementation\kem\mceliece8192128\crypto_kem_mceliece8192128.h"
#include "NTRUPrime\Optimized_Implementation\kem\sntrup857\crypto_kem_sntrup857.h"
#include "NewHope\ref\ccakem.h"

// The siphash function is defined in the siphash.c reference
// implementation file included in the SipHash folder
#ifdef __cplusplus
extern "C" {
#endif
	int siphash(const uint8_t *in, const size_t inlen, const uint8_t *k,
				uint8_t *out, const size_t outlen);
#ifdef __cplusplus
}
#endif