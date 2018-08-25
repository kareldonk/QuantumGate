// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "QuantumGate.h"

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

namespace QuantumGate
{
	void InitOpenSSLDLL() noexcept
	{
		// Load the human readable error strings for libcrypto
		ERR_load_crypto_strings();

		// Load all digest and cipher algorithms
		OpenSSL_add_all_algorithms();

		// Load config file, and other important initialization
		OPENSSL_config(NULL);
	}

	void DeInitOpenSSLDLL() noexcept
	{
		// Removes all digests and ciphers
		EVP_cleanup();

		// If you omit the next, a small leak may be left when you make 
		// use of the BIO (low level API) for e.g. base64 transformations
		CRYPTO_cleanup_all_ex_data();

		// Remove error strings
		ERR_free_strings();
	}

	void InitQuantumGateDLL() noexcept
	{
		Dbg(L"QuantumGate DLL initializing...");

		InitOpenSSLDLL();
	}

	void DeInitQuantumGateDLL() noexcept
	{
		Dbg(L"QuantumGate DLL deinitializing...");

		DeInitOpenSSLDLL();
	}
}
