// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "Random.h"

#include <assert.h>

#include "..\targetver.h"

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#include <windows.h>

#include <bcrypt.h>
#pragma comment(lib, "Bcrypt.lib")

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

static BCRYPT_ALG_HANDLE BCryptAlgorithm = NULL;

int QGCryptoInitRng()
{
	assert(BCryptAlgorithm == NULL);

	if (BCryptOpenAlgorithmProvider(&BCryptAlgorithm, BCRYPT_RNG_ALGORITHM, NULL, 0) == STATUS_SUCCESS &&
		BCryptAlgorithm != NULL)
	{
		return 1;
	}

	return 0;
}

void QGCryptoDeinitRng()
{
	assert(BCryptAlgorithm != NULL);

	if (BCryptAlgorithm != NULL)
	{
		BCryptCloseAlgorithmProvider(BCryptAlgorithm, 0);
		BCryptAlgorithm = NULL;
	}
}

int QGCryptoGetRandomBytes(unsigned char* buffer, unsigned long buffer_len)
{
	assert(buffer != NULL);

	// Should already have been initialized with QGCryptoInitRng()
	assert(BCryptAlgorithm != NULL);

	if (BCryptGenRandom(BCryptAlgorithm, buffer, buffer_len, 0) == STATUS_SUCCESS)
	{
		return 1;
	}

	return 0;
}

void randombytes(unsigned char* buffer, unsigned long buffer_len)
{
	while (QGCryptoGetRandomBytes(buffer, buffer_len) != 1)
	{}
}