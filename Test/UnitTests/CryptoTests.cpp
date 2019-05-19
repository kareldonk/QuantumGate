// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Common\Util.h"
#include "Crypto\Crypto.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(CryptoTests)
	{
	public:
		TEST_METHOD(SymmetricAlgorithms)
		{
			std::vector<String> estr =
			{
				L"A",
				L"Small string",
				L"Voting for the lesser of the evils still means that you are voting for evil! It's aggression \
				and idiotic to do so. The better and more logical option is not to vote for evil at all! \
				This is similar to how people often mention that \"government is a necessary evil.\" \
				The belief that government is a necessary evil, is a belief that evil is necessary. And I \
				don't think I have to explain to you why that belief is a very dangerous and destructive belief \
				to have. It really doesn't make sense to think that way."
			};

			std::vector<Buffer> inputbufs;
			for (const auto& str : estr)
			{
				auto len = str.size() * sizeof(String::value_type);
				Buffer inbuf(reinterpret_cast<const Byte*>(str.data()), len);
				inputbufs.insert(inputbufs.end(), std::move(inbuf));
			}

			Algorithms algs;
			algs.Hash =
			{
				Algorithm::Hash::SHA256, Algorithm::Hash::SHA512,
				Algorithm::Hash::BLAKE2S256, Algorithm::Hash::BLAKE2B512
			};

			algs.Symmetric =
			{
				Algorithm::Symmetric::AES256_GCM,
				Algorithm::Symmetric::CHACHA20_POLY1305
			};

			for (auto ha : algs.Hash)
			{
				for (auto sa : algs.Symmetric)
				{
					String secret{ L"password" };
					auto nonce = Util::GetPseudoRandomBytes(64);
					BufferView noncev(nonce.GetBytes(), nonce.GetSize());

					Crypto::SymmetricKeyData skd(Crypto::SymmetricKeyType::Derived, ha, sa,
												 Algorithm::Compression::DEFLATE);
					Crypto::SymmetricKeyData skd2(Crypto::SymmetricKeyType::Derived, ha, sa,
												  Algorithm::Compression::DEFLATE);

					Assert::AreEqual(true,
									 Crypto::GenerateSymmetricKeys(BufferView(reinterpret_cast<Byte*>(secret.data()),
																			  secret.size()), skd, skd2));

					for (const auto& input : inputbufs)
					{
						{
							Buffer eoutbuf, doutbuf;

							Assert::AreEqual(true, Crypto::Encrypt(input, eoutbuf, skd, noncev));
							Assert::AreEqual(true, Crypto::Decrypt(eoutbuf, doutbuf, skd, noncev));

							// Decrypted data must match original input
							Assert::AreEqual(true, (doutbuf == input));
						}

						{
							Buffer eoutbuf, doutbuf;

							Assert::AreEqual(true, Crypto::Encrypt(input, eoutbuf, skd2, noncev));
							Assert::AreEqual(true, Crypto::Decrypt(eoutbuf, doutbuf, skd2, noncev));

							// Decrypted data must match original input
							Assert::AreEqual(true, (doutbuf == input));
						}
					}
				}
			}
		}

		TEST_METHOD(AsymmetricAlgorithms)
		{
			Algorithms algs;

			// In this test scenario we test only
			// DH KEX algorithms in primary list
			algs.PrimaryAsymmetric =
			{
				Algorithm::Asymmetric::ECDH_SECP521R1,
				Algorithm::Asymmetric::ECDH_X25519,
				Algorithm::Asymmetric::ECDH_X448,
			};

			// In this test scenario we test only
			// KEM algorithms in secondary list
			algs.SecondaryAsymmetric =
			{
				Algorithm::Asymmetric::KEM_NTRUPRIME,
				Algorithm::Asymmetric::KEM_NEWHOPE,
				Algorithm::Asymmetric::KEM_CLASSIC_MCELIECE
			};

			for (const auto aa : algs.PrimaryAsymmetric)
			{
				Crypto::AsymmetricKeyData akd_alice(aa);
				akd_alice.SetOwner(Crypto::AsymmetricKeyOwner::Alice);

				Crypto::AsymmetricKeyData akd_bob(aa);
				akd_bob.SetOwner(Crypto::AsymmetricKeyOwner::Bob);

				// Generate keys
				Assert::AreEqual(true, Crypto::GenerateAsymmetricKeys(akd_alice));
				Assert::AreEqual(true, Crypto::GenerateAsymmetricKeys(akd_bob));

				// Public key exchange
				akd_alice.PeerPublicKey = akd_bob.LocalPublicKey;
				akd_bob.PeerPublicKey = akd_alice.LocalPublicKey;

				// Generate shared secret
				Assert::AreEqual(true, Crypto::GenerateSharedSecret(akd_alice));
				Assert::AreEqual(true, Crypto::GenerateSharedSecret(akd_bob));

				// Keys not needed anymore
				akd_alice.ReleaseKeys();
				akd_bob.ReleaseKeys();

				Assert::AreEqual(true, (akd_alice.GetKey() == nullptr &&
										akd_alice.LocalPublicKey.IsEmpty() &&
										akd_alice.PeerPublicKey.IsEmpty() &&
										akd_alice.EncryptedSharedSecret.IsEmpty()));
				Assert::AreEqual(true, (akd_bob.GetKey() == nullptr &&
										akd_bob.LocalPublicKey.IsEmpty() &&
										akd_bob.PeerPublicKey.IsEmpty() &&
										akd_bob.EncryptedSharedSecret.IsEmpty()));

				// Shared secret should match
				Assert::AreEqual(true, akd_alice.SharedSecret == akd_bob.SharedSecret);
			}

			for (const auto aa : algs.SecondaryAsymmetric)
			{
				Crypto::AsymmetricKeyData akd_alice(aa);
				akd_alice.SetOwner(Crypto::AsymmetricKeyOwner::Alice);

				Crypto::AsymmetricKeyData akd_bob(aa);
				akd_bob.SetOwner(Crypto::AsymmetricKeyOwner::Bob);

				// Generate key for Alice
				Assert::AreEqual(true, Crypto::GenerateAsymmetricKeys(akd_alice));

				// Public key exchange
				akd_bob.PeerPublicKey = akd_alice.LocalPublicKey;

				// Generate shared secret
				Assert::AreEqual(true, Crypto::GenerateSharedSecret(akd_bob));

				// Encrypted shared secret exchange
				akd_alice.EncryptedSharedSecret = akd_bob.EncryptedSharedSecret;

				// Generate shared secret
				Assert::AreEqual(true, Crypto::GenerateSharedSecret(akd_alice));

				// Keys not needed anymore
				akd_alice.ReleaseKeys();
				akd_bob.ReleaseKeys();

				Assert::AreEqual(true, (akd_alice.GetKey() == nullptr &&
										akd_alice.LocalPublicKey.IsEmpty() &&
										akd_alice.PeerPublicKey.IsEmpty() &&
										akd_alice.EncryptedSharedSecret.IsEmpty()));
				Assert::AreEqual(true, (akd_bob.GetKey() == nullptr &&
										akd_bob.LocalPublicKey.IsEmpty() &&
										akd_bob.PeerPublicKey.IsEmpty() &&
										akd_bob.EncryptedSharedSecret.IsEmpty()));

				// Shared secrets should match
				Assert::AreEqual(true, akd_alice.SharedSecret == akd_bob.SharedSecret);
			}
		}

		TEST_METHOD(HashAlgorithms)
		{
			std::vector<String> hstr =
			{
				L"A",
				L"Small string",
				L"The best way to develop and organize a truly sustainable social system is to do so around the \
				individual, taking into account his basic natural needs, and proceeding from there. In fact \
				that's what true love essentially is - respecting every individual's right to life, or in other \
				words, respecting their sovereignty. So not only should \"every village be self-sustained and \
				capable of managing its affairs even to the extent of defending itself against the whole world,\" \
				but every individual should be able to do all of that as well. A strong society derives its \
				strength from the strength of the individuals that make up that society."
			};

			std::vector<Buffer> inputbufs;
			for (const auto& str : hstr)
			{
				auto len = str.size() * sizeof(String::value_type);
				Buffer inbuf(reinterpret_cast<const Byte*>(str.data()), len);
				inputbufs.insert(inputbufs.end(), std::move(inbuf));
			}

			std::vector<String> hashes
			{
				L"5hwhynFrOxrvt9EZj4NnnEyk1ZbleSJ13WIDtJIWI30=",
				L"n7G8cPscc1lfs3kcDHGPW3/IyEiQm9iT0Cp7R7Jd3cg=",
				L"aNp/y+h+F9cXPPsJD/2v3aR/+9JqQ4/WzXr472HF4gM=",
				L"AUmy/kFc5ofXMkLPIeb3ebcMNnHg31UYOHSROw03d+Su6UtGUPAG39W0GAwZ8TYmsls4LuTtNHqPTFBWK0jruQ==",
				L"eKSZUxtDDJ6GJMrsZFBRhcjhqAFOiXbf3huUeAzPB2DCvnx6Y2j+PfWZg9B+1F4lsyEEmF/XEmDBNsCW7kTtXQ==",
				L"UvieWgZODH/tFbjUxJ9PUAWCqBqtn8S2nXatcUJjnMBMiM4q9/1obWgNGOmBprX1aXGxIlYwMz0CbEVvlUqPdg==",
				L"yZT/SSxUhOETVxTPeAbkD6gMw/vgPyIlOHfmzjOPlpE=",
				L"DV4EfdE9ELtTJboIYTISEzWk7lJNcM68Q8Ln1ImYOX0=",
				L"iZYcGT1hyHyOXvl1n4FSaFn0ikysSpdrdlmudBY4ADM=",
				L"gVp4Rgv1lQtqucq4pGCCUhfFkpRVw2jj8xfyUvTy4dwxo2Rz1JBtrvSMEJGtPQR3JIb/cDhrq5V6ZpenrxxbxQ==",
				L"8xUd5jK+CMnJRg56IDUNdxW9dUW5DOJqHAtv1wyNbEBX4PIDRXmLoejzRBFhmqjU3ivA1tDhDWC6kBg7WR+l/Q==",
				L"6ZVG2Ua8uWm0iol1a1VKgW1Y9cxmi7nuB7UTZJYODHRuRdg5OL/SgxUTTxosbJYLCN26Id6cgBPuFM3yRmn3uQ=="
			};

			Algorithms algs;

			for (auto ha : algs.Hash)
			{
				for (const auto& input : inputbufs)
				{
					Buffer houtbuf;

					Assert::AreEqual(true, Crypto::Hash(input, houtbuf, ha));

					//Dbg(L"Hash: %s", Util::ToBase64(houtbuf).second.c_str());

					Assert::AreEqual(true, std::find(hashes.begin(), hashes.end(),
													 Util::ToBase64(houtbuf).value()) != hashes.end());
				}
			}
		}

		TEST_METHOD(SignAndVerify)
		{
			std::vector<String> estr =
			{
				L"A",
				L"Small string",
				L"What matters is what you've learned about yourself and your environment from experience, "
				"and if you've been able to put that knowledge into practice (change your behavior, act on "
				"your conscience etc.) in order to improve and evolve. In other words, what ultimately matters "
				"in this reality and beyond are your achievements on a personal spiritual level (your consciousness), "
				"in order to become a better more enlightened 'idea' or soul."
			};

			std::vector<Buffer> inputbufs;
			for (const auto& str : estr)
			{
				auto len = str.size() * sizeof(String::value_type);
				Buffer inbuf(reinterpret_cast<const Byte*>(str.data()), len);
				inputbufs.insert(inputbufs.end(), std::move(inbuf));
			}

			Algorithms algs;
			algs.Hash =
			{
				Algorithm::Hash::SHA256, Algorithm::Hash::SHA512,
				Algorithm::Hash::BLAKE2S256, Algorithm::Hash::BLAKE2B512
			};

			std::vector<QuantumGate::UUID::SignAlgorithm> salgs
			{
				QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519,
				QuantumGate::UUID::SignAlgorithm::EDDSA_ED448
			};

			for (auto sa : salgs)
			{
				const auto[success, uuid, keys] = QuantumGate::UUID::Create(QuantumGate::UUID::Type::Peer, sa);
				Assert::AreEqual(true, success);

				Algorithm::Asymmetric as{ Algorithm::Asymmetric::Unknown };
				switch (uuid.GetSignAlgorithm())
				{
					case QuantumGate::UUID::SignAlgorithm::EDDSA_ED25519:
						as = Algorithm::Asymmetric::EDDSA_ED25519;
						break;
					case QuantumGate::UUID::SignAlgorithm::EDDSA_ED448:
						as = Algorithm::Asymmetric::EDDSA_ED448;
						break;
					default:
						Assert::Fail(L"Unknown digital signature algorithm.");
				}

				for (auto ha : algs.Hash)
				{
					for (const auto& input : inputbufs)
					{
						Buffer sig;
						auto retval = Crypto::HashAndSign(input, as, keys->PrivateKey, sig, ha);
						Assert::AreEqual(true, retval);
						retval = Crypto::HashAndVerify(input, as, keys->PublicKey, sig, ha);
						Assert::AreEqual(true, retval);
					}
				}
			}
		}

		TEST_METHOD(CompareBuffers)
		{
			// Empty buffers should be equal
			Buffer b1, b2;
			Assert::AreEqual(true, Crypto::CompareBuffers(b1, b2));

			Buffer::VectorType vb1{ Byte{ 0x8f }, Byte{ 0xf2 }, Byte{ 0x33 }, Byte{ 0x99 },
				Byte{ 0x00 }, Byte{ 0xdd }, Byte{ 0xee }, Byte{ 0x1e }, Byte{ 0x6f }, Byte{ 0xf7 } };

			// Same content should be equal
			b1 = vb1;
			b2 = vb1;
			Assert::AreEqual(true, Crypto::CompareBuffers(b1, b2));

			// One buffer smaller should fail
			b1.RemoveFirst(3);
			Assert::AreEqual(false, Crypto::CompareBuffers(b1, b2));

			// One buffer empty should fail
			b2.Clear();
			Assert::AreEqual(false, Crypto::CompareBuffers(b1, b2));

			Buffer::VectorType vb2{ Byte{ 0x8f }, Byte{ 0xf2 }, Byte{ 0x33 }, Byte{ 0x99 },
				Byte{ 0x00 }, Byte{ 0xdd }, Byte{ 0xee }, Byte{ 0x2e }, Byte{ 0x6f }, Byte{ 0xf7 } };

			// Different content
			b1 = vb1;
			b2 = vb2;
			Assert::AreEqual(false, Crypto::CompareBuffers(b1, b2));

			// Equal content
			b1.RemoveLast(3);
			b2.RemoveLast(3);
			Assert::AreEqual(true, Crypto::CompareBuffers(b1, b2));
		}

		TEST_METHOD(ValidateBuffer)
		{
			// Empty buffer should fail
			std::vector<UChar> ebuffer;
			Assert::AreEqual(false, Crypto::ValidateBuffer(BufferView(reinterpret_cast<Byte*>(ebuffer.data()),
																	  ebuffer.size())));

			// Buffer with all bits set to off should fail
			std::vector<UChar> vbuffer{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
			Assert::AreEqual(false, Crypto::ValidateBuffer(BufferView(reinterpret_cast<Byte*>(vbuffer.data()),
																	  vbuffer.size())));

			// Buffer with all bits set to on should fail
			std::vector<UChar> vbuffer2{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
			Assert::AreEqual(false, Crypto::ValidateBuffer(BufferView(reinterpret_cast<Byte*>(vbuffer2.data()),
																	  vbuffer2.size())));

			// These should be valid
			std::vector<std::vector<UChar>> buffers{
				{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
				{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff },
				{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
				{ 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
				{ 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },
				{ 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00 },
				{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 },
				{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01 },
				{ 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
				{ 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
				{ 0xff, 0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff },
				{ 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff },
			};

			// Random bytes should be valid
			for (auto x = 0u; x < 10u; ++x)
			{
				auto bytes = Util::GetPseudoRandomBytes(10);
				std::vector<UChar> buffer(bytes.GetSize());
				memcpy(buffer.data(), bytes.GetBytes(), bytes.GetSize());
				buffers.insert(buffers.end(), std::move(buffer));
			}

			for (auto buffer : buffers)
			{
				Assert::AreEqual(true, Crypto::ValidateBuffer(BufferView(reinterpret_cast<Byte*>(buffer.data()),
																		 buffer.size())));
			}
		}
	};
}