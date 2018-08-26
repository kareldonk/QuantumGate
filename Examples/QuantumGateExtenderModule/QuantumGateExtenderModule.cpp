// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "..\QuantumGateExtender\MinimalExtender.h"

#if defined(_DEBUG)
#if !defined(_WIN64)
#pragma comment (lib, "QuantumGate32D.lib")
#else
#pragma comment (lib, "QuantumGate64D.lib")
#endif
#else
#if !defined(_WIN64)
#pragma comment (lib, "QuantumGate32.lib")
#else
#pragma comment (lib, "QuantumGate64.lib")
#endif
#endif

// This function will get called by QuantumGate upon loading the module file. Instantiate a copy
// of one or more extenders and return them in a std::vector collection as shown below. In this example,
// we'll return the MinimalExtender from the QuantumGateExtender example project.
// Also note that this function is exported at link time using the QuantumGateExport.def file.
QuantumGate::Result<std::vector<std::shared_ptr<QuantumGate::Extender>>> MakeQuantumGateExtenders()
{
	try
	{
		// Instantiate extender
		auto extender = std::make_shared<MinimalExtender>();

		// Extender collection
		std::vector<std::shared_ptr<QuantumGate::Extender>> extenders;

		// Add the extender to the collection
		extenders.emplace_back(std::move(extender));

		// Add more extenders if needed

		// Return the extender collection to QuantumGate
		return { std::move(extenders) };
	}
	catch (...) {}

	// If anything goes wrong let QuantumGate know we failed
	return QuantumGate::ResultCode::Failed;
}