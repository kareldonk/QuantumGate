// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPMessage.h"
#include "..\..\..\QuantumGateCryptoLib\QuantumGateCryptoLib.h"

namespace QuantumGate::Implementation::Core::UDP::Listener
{
	class ConnectionCookies final
	{
		struct CookieInfo final
		{
			ConnectionID ConnectionID{ 0 };
			IPEndpoint Endpoint;
		};

		struct CookieKey final
		{
			SteadyTime CreationSteadyTime;
			UInt64 Key;
		};

	public:
		[[nodiscard]] inline bool Initialize() noexcept
		{
			return RotateKeys();
		}

		inline void Deinitialize() noexcept
		{
			for (auto& key : m_Keys)
			{
				key.reset();
			}
		}

		[[nodiscard]] std::optional<Message::CookieData> GetCookie(const ConnectionID connectionid,
																   const IPEndpoint& endpoint,
																   const SteadyTime current_steadytime,
																   const std::chrono::seconds cookie_expiration_interval) noexcept
		{
			if (CheckPrimaryKey(current_steadytime, cookie_expiration_interval))
			{
				assert(m_Keys[0].has_value());

				Message::CookieData cookie{
					.CookieID = CalcCookieID(m_Keys[0].value(), connectionid, endpoint)
				};

				return cookie;
			}

			return std::nullopt;
		}

		[[nodiscard]] bool VerifyCookie(const Message::CookieData& cookie, const ConnectionID connectionid,
										const IPEndpoint& endpoint) const noexcept
		{
			for (auto& key : m_Keys)
			{
				if (key.has_value())
				{
					const auto cookieid = CalcCookieID(key.value(), connectionid, endpoint);
					if (cookieid == cookie.CookieID) return true;
				}
			}

			return false;
		}

	private:
		[[nodiscard]] CookieID CalcCookieID(const CookieKey& cookiekey, const ConnectionID connectionid,
											const IPEndpoint& endpoint) const noexcept
		{
			CookieInfo cookieinfo;
			// Zero out padding bytes for consistent hash
			std::memset(&cookieinfo, 0, sizeof(cookieinfo));
			cookieinfo.ConnectionID = connectionid;
			cookieinfo.Endpoint = endpoint;

			CookieID cookieid{ 0 };

			siphash(reinterpret_cast<const uint8_t*>(&cookieinfo), sizeof(cookieinfo),
					reinterpret_cast<const uint8_t*>(&cookiekey.Key),
					reinterpret_cast<uint8_t*>(&cookieid), sizeof(cookieid));

			return cookieid;
		}

		[[nodiscard]] bool CheckPrimaryKey(const SteadyTime current_steadytime,
										   const std::chrono::seconds cookie_expiration_interval) noexcept
		{
			assert(m_Keys[0].has_value());

			// Check if primary key is expired and if so replace it
			if (m_Keys[0]->CreationSteadyTime + cookie_expiration_interval > current_steadytime)
			{
				return RotateKeys();
			}

			return true;
		}

		[[nodiscard]] bool RotateKeys() noexcept
		{
			const auto rnd = Crypto::GetCryptoRandomNumber();
			if (rnd)
			{
				// Replace old key 1 with new key
				m_Keys[1].emplace(CookieKey{ .CreationSteadyTime = Util::GetCurrentSteadyTime(), .Key = *rnd });

				// Key 0 (primary) becomes key 1 and the new key 1 becomes key 0
				m_Keys[0] = std::exchange(m_Keys[1], std::move(m_Keys[0]));

				return true;
			}
			else return false;
		}

	private:
		std::array<std::optional<CookieKey>, 2> m_Keys;
	};

	using ConnectionCookies_ThS = Concurrency::ThreadSafe<ConnectionCookies, std::shared_mutex>;
}