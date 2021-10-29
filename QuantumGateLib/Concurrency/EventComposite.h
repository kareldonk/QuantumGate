// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Event.h"

namespace QuantumGate::Implementation::Concurrency
{
	enum class EventCompositeOperatorType { AND, OR };

	template<std::size_t NumSubEvents, EventCompositeOperatorType OperatorType = EventCompositeOperatorType::AND>
	class EventComposite final
	{
		static_assert(NumSubEvents > 1, "Number of subevents must be greater than 1.");

		template<typename EC, typename EV>
		class SubEvent final
		{
		public:
			SubEvent() noexcept {}
			SubEvent(EC* composite, EV* event) noexcept :
				m_Composite(composite), m_Event(event) {}

			SubEvent(const SubEvent&) = delete;

			SubEvent(SubEvent&& other) noexcept :
				m_Composite(other.m_Composite), m_Event(other.m_Event)
			{
				other.m_Composite = nullptr;
				other.m_Event = nullptr;
			}

			~SubEvent() = default;
			SubEvent& operator=(const SubEvent&) = delete;

			SubEvent& operator=(SubEvent&& other) noexcept
			{
				m_Composite = std::exchange(other.m_Composite, nullptr);
				m_Event = std::exchange(other.m_Event, nullptr);

				return *this;
			}

			inline explicit operator bool() const noexcept
			{
				return (m_Composite != nullptr && m_Event != nullptr && m_Event->GetHandle() != nullptr);
			}

			template<typename EC2 = EC, typename EV2 = EV,
				typename = std::enable_if_t<!std::is_const_v<EC2> && !std::is_const_v<EV2>>>
			inline bool Set() noexcept
			{
				assert(*this);
				const bool retval = m_Event->Set();
				m_Composite->Synchronize();
				return retval;
			}

			template<typename EC2 = EC, typename EV2 = EV,
				typename = std::enable_if_t<!std::is_const_v<EC2> && !std::is_const_v<EV2>>>
			inline bool Reset() noexcept
			{
				assert(*this);
				const bool retval = m_Event->Reset();
				m_Composite->Synchronize();
				return retval;
			}

			[[nodiscard]] inline bool IsSet() const noexcept
			{
				assert(*this);
				return m_Event->IsSet();
			}

			inline void Release() noexcept
			{
				m_Composite = nullptr;
				m_Event = nullptr;
			}

		private:
			EC* m_Composite{ nullptr };
			EV* m_Event{ nullptr };
		};

	public:
		using SubEventType = SubEvent<EventComposite, Event>;
		using SubEventConstType = SubEvent<const EventComposite, const Event>;

		using HandleType = Event::HandleType;

		EventComposite() noexcept = default;
		EventComposite(const Event&) = delete;
		EventComposite(EventComposite&& other) noexcept : m_Events(std::move(other.m_Events)) {}
		~EventComposite() = default;
		EventComposite& operator=(const EventComposite&) = delete;
		EventComposite& operator=(EventComposite&&) noexcept = default;

		[[nodiscard]] inline EventCompositeOperatorType GetOperatorType() const noexcept { return OperatorType; }

		[[nodiscard]] inline Event& GetEvent() noexcept { return GetMainEvent(); }
		[[nodiscard]] inline const Event& GetEvent() const noexcept { return GetMainEvent(); }

		[[nodiscard]] inline HandleType GetHandle() const noexcept { return GetEvent(); }
		inline operator HandleType() const noexcept { return GetEvent(); }

		[[nodiscard]] inline operator Event& () noexcept { return GetMainEvent(); }
		[[nodiscard]] inline operator const Event& () const noexcept { return GetMainEvent(); }

		[[nodiscard]] inline SubEventType operator[](const int idx) noexcept { GetSubEvent(idx); }
		[[nodiscard]] inline SubEventConstType operator[](const int idx) const noexcept { GetSubEvent(idx); }

		[[nodiscard]] SubEventType GetSubEvent(const int idx) noexcept { return SubEventType(this, &m_Events[idx + 1]); }
		[[nodiscard]] SubEventConstType GetSubEvent(const int idx) const noexcept { return SubEventConstType(this, &m_Events[idx + 1]); }

		[[nodiscard]] inline bool IsSet() const noexcept { return GetMainEvent().IsSet(); }

		template<typename = std::enable_if_t<OperatorType == EventCompositeOperatorType::AND>>
		bool Set() noexcept
		{
			bool success{ true };
			for (auto& event : m_Events)
			{
				success &= event.Set();
			}
			return success;
		}

		bool Reset() noexcept
		{
			bool success{ true };
			for (auto& event : m_Events)
			{
				success &= event.Reset();
			}
			return success;
		}

		void Synchronize() noexcept
		{
			bool has_set{ false };
			bool has_unset{ false };

			// Skip first event which is the main event
			for (auto it = m_Events.begin() + 1; it != m_Events.end(); ++ it)
			{
				if (it->IsSet())
				{
					has_set = true;
					if (OperatorType == EventCompositeOperatorType::OR) break;
				}
				else has_unset = true;
			}

			if constexpr (OperatorType == EventCompositeOperatorType::AND)
			{
				if (has_set && !has_unset) GetMainEvent().Set();
				else GetMainEvent().Reset();
			}
			else
			{
				if (has_set) GetMainEvent().Set();
				else GetMainEvent().Reset();
			}
		}

	private:
		[[nodiscard]] inline Event& GetMainEvent() noexcept { return m_Events[0]; }
		[[nodiscard]] inline const Event& GetMainEvent() const noexcept { return m_Events[0]; }

	private:
		// Add extra event for the first which is the main event
		std::array<Event, 1 + NumSubEvents> m_Events;
	};
}