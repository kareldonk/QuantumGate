// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
	int QGCryptoInitRng();
	void QGCryptoDeinitRng();
	int QGCryptoGetRandomBytes(unsigned char* buffer, unsigned long buffer_len);
#ifdef __cplusplus
}
#endif

void randombytes(unsigned char* buffer, unsigned long buffer_len);
