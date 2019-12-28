// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <QuantumGate.h>
#include <Console.h>
#include <Common\Util.h>
#include "..\TestExtender\TestExtender.h"

using namespace QuantumGate;
using namespace std;

class CmdConsole final : public QuantumGate::Console::TerminalOutput
{
public:
	CmdConsole() {}
	virtual ~CmdConsole() {}

	void AddMessage(const QuantumGate::Console::MessageType type,
					const QuantumGate::WChar* message) override
	{
		TerminalOutput::AddMessage(type, message);
	}
};

bool Send(const shared_ptr<TestExtender::Extender>& extender, const wstring& pluidstr,
		  const wstring& msg, const wstring& count);
bool SetVerbosity(const wstring& verb);
void DisplayPrompt();