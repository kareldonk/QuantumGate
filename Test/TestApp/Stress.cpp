// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "Stress.h"

#include "Console.h"
#include "Common\Util.h"

#include "..\StressExtender\StressExtender.h"

using namespace QuantumGate::Implementation;

Stress::ExtenderStartupShutdownStressData Stress::m_ExtenderStartupShutdownStressData;
Stress::ConnectStressData Stress::m_ConnectStressData;
Stress::MultiInstanceStressData Stress::m_MultiInstanceStressData;

bool Stress::StartExtenderStartupShutdownStress(Local& qg)
{
	if (!m_ExtenderStartupShutdownStressData.Thread.joinable())
	{
		m_ExtenderStartupShutdownStressData.Stop = false;
		m_ExtenderStartupShutdownStressData.Thread = std::thread(Stress::ExtenderStartupShutdownStressThreadProc, &qg);
		return true;
	}

	return false;
}

void Stress::StopExtenderStartupShutdownStress()
{
	if (m_ExtenderStartupShutdownStressData.Thread.joinable())
	{
		m_ExtenderStartupShutdownStressData.Stop = true;
		m_ExtenderStartupShutdownStressData.Thread.join();
	}
}

bool Stress::IsExtenderStartupShutdownStressRunning() noexcept
{
	return m_ExtenderStartupShutdownStressData.Thread.joinable();
}

void Stress::ExtenderStartupShutdownStressThreadProc(Local* qg)
{
	LogWarn(L"Extender init/deinit stress starting...", m_ConnectStressData.IP, m_ConnectStressData.Port);

	while (!m_ExtenderStartupShutdownStressData.Stop)
	{
		if (qg->IsRunning())
		{
			if (!qg->AreExtendersEnabled())
			{
				if (!qg->EnableExtenders())
				{
					LogErr(L"Failed to enable extenders");
				}
			}
			else
			{
				if (!qg->DisableExtenders())
				{
					LogErr(L"Failed to disable extenders");
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		}
		else break;
	}
}

bool Stress::StartConnectStress(Local& qg, const CString& ip, const UInt16 port, const RelayHop hops,
								const std::optional<PeerLUID>& rpeer, const ProtectedBuffer& gsecret)
{
	if (!m_ConnectStressData.Thread.joinable())
	{
		m_ConnectStressData.Stop = false;
		m_ConnectStressData.IP = (LPCWSTR)ip;
		m_ConnectStressData.Port = port;
		m_ConnectStressData.Hops = hops;
		m_ConnectStressData.RelayPeer = rpeer;
		m_ConnectStressData.GlobalSharedSecret = gsecret;
		m_ConnectStressData.Thread = std::thread(Stress::ConnectStressThreadProc, &qg);
		return true;
	}

	return false;
}

void Stress::StopConnectStress()
{
	if (m_ConnectStressData.Thread.joinable())
	{
		m_ConnectStressData.Stop = true;
		m_ConnectStressData.Thread.join();
	}
}

bool Stress::IsConnectStressRunning() noexcept
{
	return m_ConnectStressData.Thread.joinable();
}

void Stress::ConnectStressThreadProc(Local* qg)
{
	assert(qg != nullptr);

	LogWarn(L"Connect stress starting for endpoint %s:%u...", m_ConnectStressData.IP.c_str(), m_ConnectStressData.Port);

	std::atomic<PeerLUID> pluid{ 0 };

	while (!m_ConnectStressData.Stop)
	{
		if (!m_ConnectStressData.Connected)
		{
			ConnectParameters params;
			params.PeerIPEndpoint = IPEndpoint(IPAddress(m_ConnectStressData.IP), m_ConnectStressData.Port);
			params.GlobalSharedSecret = m_ConnectStressData.GlobalSharedSecret;
			params.Relay.Hops = m_ConnectStressData.Hops;
			params.Relay.ViaPeer = m_ConnectStressData.RelayPeer;

			const auto connect_result = qg->ConnectTo(std::move(params));
			if (connect_result.Succeeded())
			{
				pluid = connect_result->PeerLUID;
				m_ConnectStressData.Connected = true;
			}
			else
			{
				LogErr(L"Connect stress: could not connect to peer (%s)", connect_result.GetErrorString().c_str());
			}
		}
		else
		{
			if (!qg->DisconnectFrom(pluid))
			{
				LogErr(L"Failed to disconnect from peer %llu", pluid.load());
			}

			pluid = 0;
			m_ConnectStressData.Connected = false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	if (pluid != 0)
	{
		if (!qg->DisconnectFrom(pluid, nullptr))
		{
			LogErr(L"Failed to disconnect from peer %llu", pluid.load());
		}
	}
}

bool Stress::StartMultiInstanceStress(const StartupParameters& startup_params,
									  const CString& ip, const UInt16 port, const ProtectedBuffer& gsecret)
{
	if (!m_MultiInstanceStressData.Thread.joinable())
	{
		m_MultiInstanceStressData.Stop = false;
		m_MultiInstanceStressData.StartupParams = startup_params;
		m_MultiInstanceStressData.IP = (LPCWSTR)ip;
		m_MultiInstanceStressData.Port = port;
		m_MultiInstanceStressData.GlobalSharedSecret = gsecret;
		m_MultiInstanceStressData.Thread = std::thread(Stress::MultiInstanceStressThreadProc);
		return true;
	}

	return false;
}

void Stress::StopMultiInstanceStress()
{
	if (m_MultiInstanceStressData.Thread.joinable())
	{
		m_MultiInstanceStressData.Stop = true;
		m_MultiInstanceStressData.Thread.join();
	}
}

bool Stress::IsMultiInstanceStressRunning() noexcept
{
	return m_MultiInstanceStressData.Thread.joinable();
}

void Stress::MultiInstanceStressThreadProc()
{
	LogWarn(L"Multi instance stress starting for endpoint %s:%u...",
			m_MultiInstanceStressData.IP.c_str(), m_MultiInstanceStressData.Port);

	const std::array<std::wstring, 5> messages =
	{
		L"What is contrary to the visible truth must change or disappear -- that's the law of life",
		L"I shall never believe that what is founded on lies can endure for ever. I believe in truth. I'm sure that, in the long run, truth must be victorious.",
		L"Research must remain free and unfettered by any State restriction. The facts which it establishes represent Truth, and Truth is never evil.",
		L"The man of research is by nature extremely cautious; he never ceases to work, to ponder, to weigh and to doubt, and his suspicious nature breeds in him an inclination towards solitude and most rigorous self-criticism.",
		L"Adolf Hitler, from 'Hitler's Table Talk, 1941-1944: His Private Conversations'"
	};

	auto error = false;

	std::vector<std::shared_ptr<StressExtender::Extender>> extenders;
	std::vector<Local> instances;

	for (auto x = 0u; x < 10u; ++x)
	{
		auto& local = instances.emplace_back();

		auto extender = std::make_shared<StressExtender::Extender>();

		if (local.AddExtender(extender))
		{
			extenders.push_back(extender);

			local.GetAccessManager().SetPeerAccessDefault(QuantumGate::PeerAccessDefault::Allowed);

			if (const auto result = local.Startup(m_MultiInstanceStressData.StartupParams); result.Failed())
			{
				LogErr(L"Failed to start a QuantumGate instance: " + result.GetErrorString());

				error = true;
				break;
			}
		}
		else
		{
			LogErr(L"Failed to add extender to a QuantumGate instance");

			error = true;
			break;
		}
	}

	if (!error)
	{
		Vector<PeerLUID> pluids;

		while (!m_MultiInstanceStressData.Stop)
		{
			for (auto& instance : instances)
			{
				ConnectParameters params;
				params.PeerIPEndpoint = IPEndpoint(IPAddress(m_MultiInstanceStressData.IP), m_MultiInstanceStressData.Port);
				params.GlobalSharedSecret = m_MultiInstanceStressData.GlobalSharedSecret;

				const auto connect_result = instance.ConnectTo(std::move(params), nullptr);
				if (connect_result.Failed())
				{
					LogErr(L"Connect instance stress: could not connect to peer (%s)",
						   connect_result.GetErrorString().c_str());
				}
			}

			for (std::size_t x = 0; x < instances.size(); ++x)
			{
				PeerQueryParameters qparams;
				const auto result = instances[x].QueryPeers(qparams, pluids);
				if (result.Succeeded() && pluids.size() > 0)
				{
					const auto num_msg = Util::GetPseudoRandomNumber(1, 5);
					for (Int64 y = 0; y < num_msg; ++y)
					{
						extenders[x]->SendMessageW(pluids[0],
												   messages[static_cast<size_t>(Util::GetPseudoRandomNumber(0, messages.size() - 1))]);
					}
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	for (auto& instance : instances)
	{
		if (instance.IsRunning()) DiscardReturnValue(instance.Shutdown());
	}
}