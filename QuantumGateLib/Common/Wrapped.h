// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation
{
	template<typename T>
	class Wrapped
	{
		static_assert(!std::is_pointer_v<T> && !std::is_reference_v<T>, "Wrapped type should not be a pointer or reference.");

		using BaseType = std::decay_t<T>;

	public:
		using ValueType = T;

		constexpr Wrapped() noexcept : Wrapped(nullptr) {}
		
		constexpr Wrapped(std::nullptr_t) noexcept : m_IsPointer(true), m_Pointer(nullptr) {}

		constexpr Wrapped(T* ptr) noexcept : m_IsPointer(true), m_Pointer(ptr) {}
	
		constexpr Wrapped(const BaseType& data) noexcept(noexcept(Construct(data)))
		{
			Construct(data);
		}
		
		constexpr Wrapped(BaseType&& data) noexcept(noexcept(Construct(std::move(data))))
		{
			Construct(std::move(data));
		}

		constexpr Wrapped(const Wrapped& other) noexcept(noexcept(Construct(other.m_Data)))
		{
			if (other.m_IsPointer)
			{
				Reset(other.m_Pointer);
			}
			else
			{
				Construct(other.m_Data);
			}
		}

		constexpr Wrapped(Wrapped&& other) noexcept(noexcept(Construct(std::move(other.m_Data))))
		{
			if (other.m_IsPointer)
			{
				Reset(other.m_Pointer);
			}
			else
			{
				Construct(std::move(other.m_Data));
			}
		}
		
		constexpr ~Wrapped() requires (std::is_trivially_destructible_v<T>) = default;

		constexpr ~Wrapped() requires (!std::is_trivially_destructible_v<T>)
		{
			Reset();
		}

		constexpr Wrapped& operator=(const Wrapped& other) noexcept(noexcept(Assign(other.m_Data)))
			requires (!std::is_const_v<T>)
		{
			if (this == &other) return *this;

			if (other.m_IsPointer)
			{
				Reset(other.m_Pointer);
			}
			else
			{
				Assign(other.m_Data);
			}

			return *this;
		}

		constexpr Wrapped& operator=(Wrapped&& other) noexcept(noexcept(Assign(std::move(other.m_Data))))
			requires (!std::is_const_v<T>)
		{
			if (this == &other) return *this;

			if (other.m_IsPointer)
			{
				Reset(other.m_Pointer);
			}
			else
			{
				Assign(std::move(other.m_Data));
			}
			
			return *this;
		}

		constexpr Wrapped& operator=(const BaseType& data) noexcept(noexcept(Assign(data)))
			requires (!std::is_const_v<T>)
		{
			if (std::addressof(data) == std::addressof(m_Data)) return *this;
			
			Assign(data);
			return *this;
		}

		constexpr Wrapped& operator=(BaseType&& data) noexcept(noexcept(Assign(std::move(data))))
			requires (!std::is_const_v<T>)
		{
			if (std::addressof(data) == std::addressof(m_Data)) return *this;

			Assign(std::move(data));
			return *this;
		}

		constexpr Wrapped& operator=(T* ptr) noexcept
		{
			Reset(ptr);
			return *this;
		}

		[[nodiscard]] constexpr inline bool IsOwner() const noexcept
		{
			return !m_IsPointer;
		}

		constexpr inline explicit operator bool() const noexcept
		{
			if (m_IsPointer) return (m_Pointer != nullptr);
			else return true;
		}

		constexpr inline std::conditional_t<std::is_const_v<T>, const T*, T*> operator->() noexcept
		{
			assert(*this);
			if (m_IsPointer) return m_Pointer;
			else return &m_Data;
		}

		constexpr inline const T* operator->() const noexcept
		{
			assert(*this);
			if (m_IsPointer) return m_Pointer;
			else return &m_Data;
		}

		constexpr inline std::conditional_t<std::is_const_v<T>, const T&, T&> operator*() noexcept
		{
			assert(*this);
			if (m_IsPointer) return *m_Pointer;
			else return m_Data;
		}

		constexpr inline const T& operator*() const noexcept
		{
			assert(*this);
			if (m_IsPointer) return *m_Pointer;
			else return m_Data;
		}

		template<typename Arg> requires (!std::is_const_v<T>)
		constexpr inline auto& operator[](Arg&& arg) noexcept(noexcept(std::declval<T>().operator[](std::forward<Arg>(arg))))
		{
			assert(*this);
			if (m_IsPointer) return (*m_Pointer)[std::forward<Arg>(arg)];
			else return m_Data[std::forward<Arg>(arg)];
		}

		template<typename Arg> requires (std::is_const_v<T>)
		constexpr inline const auto& operator[](Arg&& arg) const noexcept(noexcept(std::declval<T>().operator[](std::forward<Arg>(arg))))
		{
			assert(*this);
			if (m_IsPointer) return (*m_Pointer)[std::forward<Arg>(arg)];
			else return m_Data[std::forward<Arg>(arg)];
		}

		template<typename... Args>
		constexpr inline T& Emplace(Args... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
		{
			Destroy();
			Construct(std::forward<Args>(args)...);
			return m_Data;
		}

		constexpr inline void Reset() noexcept
		{
			Reset(nullptr);
		}

	private:
		template<typename... Args>
		constexpr inline void Construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
		{
			std::construct_at(std::addressof(m_Data), std::forward<Args>(args)...);
			m_IsPointer = false;
		}

		template<typename D>
		constexpr inline void Assign(D&& data) noexcept(noexcept(Construct(std::forward<D>(data))) && noexcept(m_Data = std::forward<D>(data)))
		{
			if (m_IsPointer)
			{
				Construct(std::forward<D>(data));
			}
			else
			{
				m_Data = std::forward<D>(data);
			}
		}

		constexpr inline void Destroy() noexcept
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				if (!m_IsPointer)
				{
					m_Data.~BaseType();
				}
			}
		}

		constexpr inline void Reset(T* ptr) noexcept
		{
			Destroy();
			m_IsPointer = true;
			m_Pointer = ptr;
		}

	private:
		bool m_IsPointer;
		union
		{
			T* m_Pointer;
			T m_Data;
		};
	};
}