// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Memory\Allocator.h"

#include <queue>
#include <map>
#include <list>
#include <unordered_map>
#include <unordered_set>

namespace QuantumGate::Implementation::Containers
{
	template<typename T>
	using Deque = std::deque<T, Memory::DefaultAllocator<T>>;

	template<typename T>
	using Queue = std::queue<T, Deque<T>>;

	template <typename T, typename C = Vector<T>, typename Pred = std::less<typename C::value_type>>
	using PriorityQueue = std::priority_queue<T, C, Pred>;

	template<typename Key, typename T, typename C = std::less<Key>>
	using Map = std::map<Key, T, C, Memory::DefaultAllocator<std::pair<const Key, T>>>;

	template<typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
	using UnorderedMap = std::unordered_map<Key, T, Hash, KeyEqual, Memory::DefaultAllocator<std::pair<const Key, T>>>;

	template<typename T>
	using List = std::list<T, Memory::DefaultAllocator<T>>;

	template <typename Key, typename Hash = std::hash<Key>, typename Pred = std::equal_to<Key>>
	using UnorderedSet = std::unordered_set<Key, Hash, Pred, Memory::DefaultAllocator<Key>>;
}