// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\MessageTypes.h"

namespace QuantumGate::Implementation::Core::Peer
{
	class MessageDetails final
	{
	public:
		MessageDetails(const MessageType type, const ExtenderUUID& extuuid, Buffer&& msgdata) noexcept :
			m_MessageType(type), m_ExtenderUUID(extuuid), m_MessageData(std::move(msgdata))
		{}

		MessageDetails(const MessageDetails&) = delete;
		MessageDetails(MessageDetails&&) noexcept = default;
		~MessageDetails() = default;
		MessageDetails& operator=(const MessageDetails&) = delete;
		MessageDetails& operator=(MessageDetails&&) noexcept = default;

		[[nodiscard]] inline bool AddToMessageData(const Buffer& data) noexcept
		{
			try
			{
				m_MessageData += data;
				return true;
			}
			catch (...) {}

			return false;
		}

		inline MessageType GetMessageType() const noexcept { return m_MessageType; }
		inline const ExtenderUUID& GetExtenderUUID() const noexcept { return m_ExtenderUUID; }
		inline const Buffer& GetMessageData() const noexcept { return m_MessageData; }

	private:
		MessageType m_MessageType{ MessageType::Unknown };
		ExtenderUUID m_ExtenderUUID;
		Buffer m_MessageData;
	};
}