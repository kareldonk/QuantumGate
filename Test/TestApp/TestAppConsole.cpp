// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "TestAppConsole.h"

#include "Common\Util.h"

#define QGCONSOLE_MAX_MESSAGESIZE	32 * 1024 // Number of characters

using namespace QuantumGate::Implementation;

void TestAppConsole::AddMessage(const QuantumGate::Console::MessageType type,
								const QuantumGate::WChar* message)
{
	std::unique_lock<std::mutex> lock(m_Mutex);

	// If number of messages in console becomes too much erase some of the history
	while (m_Messages.size() > QGCONSOLE_MAX_MESSAGESIZE)
	{
		const auto pos = m_Messages.find(L"\r\n");
		if (pos != QuantumGate::String::npos)
		{
			m_Messages.erase(0, pos + 2);
		}
		else break;
	}

	if (type == QuantumGate::Console::MessageType::Warning) m_Messages += L"! ";
	else if (type == QuantumGate::Console::MessageType::Error) m_Messages += L"* ";
	else m_Messages += L"  ";

	if (type != QuantumGate::Console::MessageType::System)
	{
		m_Messages += L"[" + Util::GetCurrentLocalTime(L"%d/%m/%Y %H:%M:%S") + L"] ";
	}

	m_Messages += message;
	m_Messages += L"\r\n";

	m_NewMessageEvent.Set();

#ifdef _DEBUG
	// Additionally send debug messages to the debug output
	if (type == QuantumGate::Console::MessageType::Debug) Dbg(message);
#endif
}
