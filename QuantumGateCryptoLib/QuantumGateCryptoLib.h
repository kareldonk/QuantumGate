// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Common\nist\rng.h"
#include "McEliece\Optimized_Implementation\kem\mceliece8192128\crypto_kem_mceliece8192128.h"
#include "NTRUPrime\Optimized_Implementation\kem\sntrup4591761\crypto_kem_sntrup4591761.h"
#include "NewHope\Optimized_Implementation\crypto_kem\newhope1024cca\api.h"

// The siphash function is defined in the siphash.c reference implementation file
// included in the SipHash folder, and available online at: https://github.com/veorq/SipHash
#ifdef __cplusplus
extern "C" {
#endif
	extern int siphash(const uint8_t *in, const size_t inlen, const uint8_t *k,
					   uint8_t *out, const size_t outlen);
#ifdef __cplusplus
}
#endif