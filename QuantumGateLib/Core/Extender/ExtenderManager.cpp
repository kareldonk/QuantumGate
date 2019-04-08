// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "ExtenderManager.h"
#include "..\Peer\PeerManager.h"
#include "..\..\API\PeerEvent.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Core::Extender
{
	Manager::Manager(const Settings_CThS& settings) noexcept : m_Settings(settings)
	{}

	bool Manager::Startup() noexcept
	{
		if (m_Running) return true;

		LogSys(L"Extendermanager starting...");

		if (!StartExtenders())
		{
			ShutdownExtenders();

			LogErr(L"Extendermanager startup failed");

			return false;
		}

		LogSys(L"Extendermanager startup successful");

		m_Running = true;

		return true;
	}

	void Manager::Shutdown() noexcept
	{
		if (!m_Running) return;

		LogSys(L"Extendermanager shutting down...");

		m_Running = false;

		ShutdownExtenders();

		LogSys(L"Extendermanager shut down");
	}

	bool Manager::StartExtenders() noexcept
	{
		auto success = true;

		try
		{
			Vector<ExtenderUUID> startupext_list;
			startupext_list.reserve(m_Extenders.WithSharedLock()->size());

			m_Extenders.WithUniqueLock([&](ExtenderMap& extenders)
			{
				// Notify extenders of startup
				for (const auto& e : extenders)
				{
					auto startup = false;

					e.second->WithSharedLock([&](const Control& extctrl) noexcept
					{
						assert(extctrl.GetStatus() == Control::Status::Stopped);

						if (extctrl.HasExtender()) startup = true;
					});

					if (startup && StartExtender(*e.second, false))
					{
						startupext_list.emplace_back(e.second->WithSharedLock()->GetExtender().GetUUID());
					}
				}

				// Needs to be done before calling update callbacks
				UpdateActiveExtenderUUIDs(extenders);
			});

			if (!startupext_list.empty())
			{
				// Let connected peers know we have added extenders.
				// We shouldn't hold locks to extenders or extender controls
				// before this call to avoid deadlock.
				m_ExtenderUpdateCallbacks.WithUniqueLock()(startupext_list, true);
			}

			m_Extenders.WithUniqueLock([&](ExtenderMap& extenders) noexcept
			{
				for (const auto& e : extenders)
				{
					e.second->WithUniqueLock([&](Control& extctrl) noexcept
					{
						if (extctrl.HasExtender() &&
							extctrl.GetStatus() == Control::Status::Running)
						{
							// Extender is now running and ready to be used
							extctrl.GetExtender().OnEndStartup();
						}
					});
				}
			});
		}
		catch (const std::exception& e)
		{
			LogErr(L"Extendermanager encountered an exception while starting extenders - %s",
				   Util::ToStringW(e.what()).c_str());
			success = false;
		}

		return success;
	}

	void Manager::ShutdownExtenders() noexcept
	{
		try
		{
			Vector<ExtenderUUID> shutdownext_list;
			shutdownext_list.reserve(m_Extenders.WithSharedLock()->size());

			m_Extenders.WithUniqueLock([&](ExtenderMap& extenders)
			{
				// Notify extenders of shutting down
				for (const auto& e : extenders)
				{
					auto shutdown = false;

					e.second->WithSharedLock([&](const Control& extctrl) noexcept
					{
						if (extctrl.HasExtender() && extctrl.GetStatus() != Control::Status::Stopped)
						{
							shutdown = true;
						}
					});

					if (shutdown)
					{
						DiscardReturnValue(ShutdownExtender(*e.second, false));
						
						shutdownext_list.emplace_back(e.second->WithSharedLock()->GetExtender().GetUUID());
					}
				}

				// Needs to be done before calling update callbacks
				UpdateActiveExtenderUUIDs(extenders);
			});

			if (!shutdownext_list.empty())
			{
				// Let connected peers know we have removed extenders.
				// We shouldn't hold locks to extenders or extender controls
				// before this call to avoid deadlock.
				m_ExtenderUpdateCallbacks.WithUniqueLock()(shutdownext_list, false);
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Extendermanager encountered an exception while shutting down extenders - %s",
				   Util::ToStringW(e.what()).c_str());
		}
	}

	Result<Control_ThS*> Manager::GetExtenderControl(const std::shared_ptr<QuantumGate::API::Extender>& extender,
													 const std::optional<ExtenderModuleID> moduleid) const noexcept
	{
		auto result_code = ResultCode::Failed;
		Control_ThS* extctrl_ths{ nullptr };

		m_Extenders.WithUniqueLock([&](const ExtenderMap& extenders) noexcept
		{
			if (const auto it = extenders.find(extender->GetUUID()); it != extenders.end())
			{
				extctrl_ths = it->second.get();
				extctrl_ths->WithUniqueLock([&](Control& extctrl) noexcept
				{
					if (extctrl.HasExtender())
					{
						if (moduleid.has_value())
						{
							// Should be same object
							if (extctrl.IsSameExtender(extender, *moduleid))
							{
								result_code = ResultCode::Succeeded;
							}
							else result_code = ResultCode::ExtenderObjectDifferent;
						}
						else result_code = ResultCode::Succeeded;
					}
					else result_code = ResultCode::ExtenderAlreadyRemoved;
				});
			}
			else result_code = ResultCode::ExtenderNotFound;
		});

		if (result_code == ResultCode::Succeeded) return extctrl_ths;

		return result_code;
	}

	Result<bool> Manager::AddExtender(const std::shared_ptr<QuantumGate::API::Extender>& extender,
									  const ExtenderModuleID moduleid) noexcept
	{
		assert(extender != nullptr);

		if (extender == nullptr) return ResultCode::InvalidArgument;

		auto result_code = ResultCode::Failed;
		const auto extname = Control::GetExtenderName(*extender->m_Extender);

		try
		{
			LogDbg(L"Adding extender %s", extname.c_str());

			auto extctrl = std::make_unique<Control_ThS>(*this, extender, moduleid);
			Control_ThS* extctrl_ths = extctrl.get();

			m_Extenders.WithUniqueLock([&](ExtenderMap& extenders)
			{
				if (const auto it = extenders.find(extender->GetUUID()); it != extenders.end())
				{
					// If extender already existed in the map replace it;
					// existing extender should have been removed already
					if (!it->second->WithSharedLock()->HasExtender())
					{
						it->second.reset(extctrl.release());
						result_code = ResultCode::Succeeded;
					}
					else
					{
						LogErr(L"Could not add extender %s; extender already present", extname.c_str());
						result_code = ResultCode::ExtenderAlreadyPresent;
					}
				}
				else
				{
					// If extender didn't exist in the map add it
					if (extenders.size() < Manager::MaximumNumberOfExtenders)
					{
						[[maybe_unused]] const auto[eit, inserted] =
							extenders.insert({ extender->GetUUID(), std::move(extctrl) });
						if (inserted)
						{
							result_code = ResultCode::Succeeded;
						}
					}
					else
					{
						LogErr(L"Could not add extender %s; maximum of %u extenders reached",
							   extname.c_str(), Manager::MaximumNumberOfExtenders);
						result_code = ResultCode::ExtenderTooMany;
					}
				}
			});

			if (result_code == ResultCode::Succeeded)
			{
				auto started = false;

				if (IsRunning())
				{
					// If we're running, start the extender
					started = StartExtender(*extctrl_ths, true);
				}

				return started;
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Extendermanager encountered an exception while adding extender %s - %s",
				   extname.c_str(), Util::ToStringW(e.what()).c_str());
		}

		return result_code;
	}

	bool Manager::StartExtender(Control_ThS& extctrl_ths, const bool update_active)
	{
		auto success = false;
		const auto extname = Control::GetExtenderName(extctrl_ths.WithSharedLock()->GetExtender());

		extctrl_ths.WithUniqueLock([&](Control& extctrl) noexcept
		{
			if (extctrl.GetStatus() != Control::Status::Stopped) return;

			LogSys(L"Extender %s starting...", extname.c_str());

			if (extctrl.GetExtender().OnBeginStartup())
			{
				extctrl.SetStatus(Control::Status::Startup);

				if (extctrl.StartupExtenderThreadPools())
				{
					extctrl.SetStatus(Control::Status::Running);
					success = true;
				}
				else
				{
					extctrl.GetExtender().OnBeginShutdown();
					extctrl.ShutdownExtenderThreadPools();
					extctrl.GetExtender().OnEndShutdown();
					extctrl.SetStatus(Control::Status::Stopped);
				}
			}
		});

		if (success)
		{
			if (update_active)
			{
				m_Extenders.WithUniqueLock([&](ExtenderMap& extenders) noexcept
				{
					// Needs to be done before calling update callbacks
					UpdateActiveExtenderUUIDs(extenders);
				});

				const Vector<ExtenderUUID> extuuids{ extctrl_ths.WithSharedLock()->GetExtender().GetUUID() };

				// Let connected peers know we have added an extender.
				// We shouldn't hold locks to extenders or extender controls
				// before this call to avoid deadlock.
				m_ExtenderUpdateCallbacks.WithUniqueLock()(extuuids, true);

				// Extender is now initialized and ready to be used
				extctrl_ths.WithUniqueLock()->GetExtender().OnEndStartup();
			}

			LogSys(L"Extender %s startup successful", extname.c_str());
		}
		else LogErr(L"Extender %s startup failed", extname.c_str());

		return success;
	}

	Result<> Manager::RemoveExtender(const std::shared_ptr<QuantumGate::API::Extender>& extender,
									 const ExtenderModuleID moduleid) noexcept
	{
		assert(extender != nullptr);

		if (extender == nullptr) return ResultCode::InvalidArgument;

		auto result_code = ResultCode::Failed;
		const auto extname = Control::GetExtenderName(*extender->m_Extender);

		try
		{
			LogDbg(L"Removing extender %s", extname.c_str());

			const auto result = GetExtenderControl(extender, moduleid);
			if (result.Succeeded())
			{
				// First shut down extender if it's running
				DiscardReturnValue(ShutdownExtender(*(result.GetValue()), true));

				result.GetValue()->WithUniqueLock()->ReleaseExtender();

				result_code = ResultCode::Succeeded;
			}
			else
			{
				result_code = static_cast<ResultCode>(result.GetErrorValue());

				switch (result_code)
				{
					case ResultCode::ExtenderObjectDifferent:
						LogErr(L"Could not remove extender %s; extender object is different", extname.c_str());
						break;
					case ResultCode::ExtenderAlreadyRemoved:
						LogErr(L"Could not remove extender %s; extender already removed", extname.c_str());
						break;
					case ResultCode::ExtenderNotFound:
						LogErr(L"Could not remove extender %s; extender not found", extname.c_str());
						break;
					default:
						assert(false);
						break;
				}
			}
		}
		catch (const std::exception& e)
		{
			LogErr(L"Extendermanager encountered an exception while removing extender %s - %s",
				   extname.c_str(), Util::ToStringW(e.what()).c_str());
		}

		return result_code;
	}

	bool Manager::ShutdownExtender(Control_ThS& extctrl_ths, const bool update_active)
	{
		auto success = false;
		const auto extname = Control::GetExtenderName(extctrl_ths.WithSharedLock()->GetExtender());

		extctrl_ths.WithUniqueLock([&](Control& extctrl) noexcept
		{
			if (extctrl.GetStatus() != Control::Status::Stopped)
			{
				LogSys(L"Extender %s shutting down...", extname.c_str());

				// Set status so that extender stops getting used;
				// we'll actually shut it down safely later below
				extctrl.SetStatus(Control::Status::Shutdown);

				success = true;
			}
		});

		if (success)
		{
			if (update_active)
			{
				m_Extenders.WithUniqueLock([&](ExtenderMap& extenders) noexcept
				{
					// Needs to be done before calling update callbacks
					UpdateActiveExtenderUUIDs(extenders);
				});
			}

			Vector<ExtenderUUID> extuuids;

			// Now we actually shut down the extender
			extctrl_ths.WithUniqueLock([&](Control& extctrl)
			{
				if (update_active)
				{
					extuuids.emplace_back(extctrl.GetExtender().GetUUID());
				}

				extctrl.GetExtender().OnBeginShutdown();
				extctrl.ShutdownExtenderThreadPools();
				extctrl.GetExtender().OnEndShutdown();
				extctrl.SetStatus(Control::Status::Stopped);

				LogSys(L"Extender %s shut down", extname.c_str());
			});

			if (update_active)
			{
				// Let connected peers know we have removed an extender.
				// We shouldn't hold locks to extenders or extender controls
				// before this call to avoid deadlock.
				m_ExtenderUpdateCallbacks.WithUniqueLock()(extuuids, false);
			}
		}

		return success;
	}

	Result<> Manager::StartExtender(const ExtenderUUID& extuuid) noexcept
	{
		auto result_code = ResultCode::Failed;

		const auto extender = GetExtender(extuuid).lock();
		if (extender)
		{
			const auto extname = Control::GetExtenderName(*extender->m_Extender);

			const auto result = GetExtenderControl(extender);
			if (result.Succeeded())
			{
				try
				{
					if (StartExtender(*(result.GetValue()), true))
					{
						result_code = ResultCode::Succeeded;
					}
				}
				catch (const std::exception& e)
				{
					LogErr(L"Extendermanager encountered an exception while starting extender %s - %s",
							extname.c_str(), Util::ToStringW(e.what()).c_str());
				}
			}
			else
			{
				result_code = static_cast<ResultCode>(result.GetErrorValue());

				switch (result_code)
				{
					case ResultCode::ExtenderAlreadyRemoved:
						LogErr(L"Could not start extender %s; extender already removed", extname.c_str());
						break;
					case ResultCode::ExtenderNotFound:
						LogErr(L"Could not start extender %s; extender not found", extname.c_str());
						break;
					default:
						assert(false);
						break;
				}
			}
		}
		else
		{
			result_code = ResultCode::ExtenderNotFound;
			LogErr(L"Could not start extender with UUID %s; extender not found", extuuid.GetString().c_str());
		}

		return result_code;
	}

	Result<> Manager::ShutdownExtender(const ExtenderUUID& extuuid) noexcept
	{
		auto result_code = ResultCode::Failed;

		const auto extender = GetExtender(extuuid).lock();
		if (extender)
		{
			const auto extname = Control::GetExtenderName(*extender->m_Extender);

			const auto result = GetExtenderControl(extender);
			if (result.Succeeded())
			{
				try
				{
					if (ShutdownExtender(*(result.GetValue()), true))
					{
						result_code = ResultCode::Succeeded;
					}
				}
				catch (const std::exception& e)
				{
					LogErr(L"Extendermanager encountered an exception while shutting down extender %s - %s",
							extname.c_str(), Util::ToStringW(e.what()).c_str());
				}
			}
			else
			{
				result_code = static_cast<ResultCode>(result.GetErrorValue());

				switch (result_code)
				{
					case ResultCode::ExtenderAlreadyRemoved:
						LogErr(L"Could not shut down extender %s; extender already removed", extname.c_str());
						break;
					case ResultCode::ExtenderNotFound:
						LogErr(L"Could not shut down extender %s; extender not found", extname.c_str());
						break;
					default:
						assert(false);
						break;
				}
			}
		}
		else
		{
			result_code = ResultCode::ExtenderNotFound;
			LogErr(L"Could not shut down extender with UUID %s; extender not found", extuuid.GetString().c_str());
		}

		return result_code;
	}

	bool Manager::HasExtender(const ExtenderUUID& extuuid) const noexcept
	{
		auto found = false;

		m_Extenders.WithSharedLock([&](const ExtenderMap& extenders)
		{
			if (const auto it = extenders.find(extuuid); it != extenders.end())
			{
				found = it->second->WithSharedLock()->HasExtender();
			}
		});

		return found;
	}

	std::weak_ptr<QuantumGate::API::Extender> Manager::GetExtender(const ExtenderUUID& extuuid) const noexcept
	{
		std::weak_ptr<QuantumGate::API::Extender> retval;

		m_Extenders.WithSharedLock([&](const ExtenderMap& extenders) noexcept
		{
			if (const auto it = extenders.find(extuuid); it != extenders.end())
			{
				it->second->WithSharedLock([&](const Control& extctrl) noexcept
				{
					auto& extender = extctrl.GetAPIExtender();
					if (extender != nullptr)
					{
						retval = extender;
					}
				});
			}
		});

		return retval;
	}

	const Settings& Manager::GetSettings() const noexcept
	{
		return m_Settings.GetCache();
	}

	void Manager::OnPeerEvent(const Vector<ExtenderUUID>& extuuids, Peer::Event&& event) noexcept
	{
		assert(event.GetType() == PeerEventType::Connected ||
			   event.GetType() == PeerEventType::Disconnected);

		m_Extenders.WithSharedLock([&](const ExtenderMap& extenders) noexcept
		{
			for (const auto& extuuid : extuuids)
			{
				// Do/did we have the extender running locally?
				if (const auto it = extenders.find(extuuid); it != extenders.end())
				{
					it->second->WithUniqueLock([&](Control& extctrl)
					{
						// If extender exists and is running let it process the event
						if (extctrl.GetStatus() == Control::Status::Running)
						{
							// Note the copy
							auto eventc = event;
							if (!extctrl.AddPeerEvent(std::move(eventc)))
							{
								LogErr(L"Failed to add peer event to extender %s", extctrl.GetExtenderName().c_str());
							}
						}
					});
				}
			}
		});
	}

	const std::pair<bool, bool> Manager::OnPeerMessage(Peer::Event&& event) noexcept
	{
		assert(event.GetType() == PeerEventType::Message);

		// Default return value (not handled, unsuccessful)
		auto retval = std::make_pair(false, false);

		m_Extenders.WithSharedLock([&](const ExtenderMap& extenders)
		{
			// Do/did we have the extender running locally?
			if (const auto it = extenders.find(*event.GetExtenderUUID()); it != extenders.end())
			{
				it->second->WithUniqueLock([&](Control& extctrl)
				{
					// If extender exists and is running let it process the message
					if (extctrl.GetStatus() == Control::Status::Running)
					{
						if (extctrl.AddPeerEvent(std::move(event)))
						{
							// Return (handled, successful)
							retval.first = true;
							retval.second = true;
						}
						else LogErr(L"Failed to add peer event to extender %s", extctrl.GetExtenderName().c_str());
					}
					else
					{
						// If the extender was not running, keep unsuccessfully handling messages
						// for a grace period so that the connection doesn't get closed. (Peers might still
						// think the extender is running locally while an extender update message is in transit.)
						if ((Util::GetCurrentSteadyTime() - extctrl.GetSteadyTimeRemoved()) <=
							m_Settings->Message.ExtenderGracePeriod)
						{
							// Return (handled, unsuccessful)
							retval.first = true;
						}
						else
						{
							LogErr(L"MessageTransport for extender with UUID %s timed out (arrived outside of grace period)",
								   event.GetExtenderUUID()->GetString().c_str());
						}
					}
				});
			}
			else
			{
				LogErr(L"Received a message for extender with UUID %s that's not running locally",
					   event.GetExtenderUUID()->GetString().c_str());
			}
		});

		return retval;
	}

	void Manager::UpdateActiveExtenderUUIDs(const ExtenderMap& extenders) noexcept
	{
		try
		{
			m_ActiveExtenderUUIDs.UpdateValue([&](ActiveExtenderUUIDs& extuuids)
			{
				extuuids.UUIDs.clear();
				extuuids.SerializedUUIDs.clear();

				for (const auto& e : extenders)
				{
					if (e.second->WithSharedLock()->GetStatus() == Control::Status::Running)
					{
						extuuids.UUIDs.emplace_back(e.first);
						extuuids.SerializedUUIDs.push_back(e.first);
					}
				}
			});
		}
		catch (const std::exception& e)
		{
			LogErr(L"Extendermanager encountered an exception while updating the active extender list - %s",
				   Util::ToStringW(e.what()).c_str());
		}
	}

	const ActiveExtenderUUIDs& Manager::GetActiveExtenderUUIDs() const noexcept
	{
		return m_ActiveExtenderUUIDs.GetCache();
	}
}
