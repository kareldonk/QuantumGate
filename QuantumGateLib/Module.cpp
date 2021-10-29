// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Module.h"

#include "..\QuantumGateCryptoLib\QuantumGateCryptoLib.h"

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

namespace QuantumGate
{
	int InitOpenSSL() noexcept
	{
		return OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
	}

	void DeinitOpenSSL() noexcept
	{
		OPENSSL_cleanup();
	}

	void InitQuantumGateModule() noexcept
	{
		Dbg(L"QuantumGate module initializing...");

		if (QGCryptoInitRng() != 1)
		{
			Dbg(L"WARNING: QGCryptoInitRng() failed");
			abort();
		}

		if (InitOpenSSL() != 1)
		{
			Dbg(L"WARNING: InitOpenSSL() failed");
			abort();
		}
	}

	void DeinitQuantumGateModule() noexcept
	{
		Dbg(L"QuantumGate module deinitializing...");

		DeinitOpenSSL();

		QGCryptoDeinitRng();
	}
}
