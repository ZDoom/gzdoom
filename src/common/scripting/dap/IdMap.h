#pragma once
#include "IdProvider.h"

#include <unordered_map>

namespace DebugServer
{
template <typename T> class IdMap
{
	std::shared_ptr<IdProvider> m_idProvider;
	std::unordered_map<uint32_t, T> m_idsToElements;
	std::unordered_map<T, uint32_t> m_elementsToIds;

	std::recursive_mutex m_elementsMutex;
	public:
	explicit IdMap(const std::shared_ptr<IdProvider> idProvider) : m_idProvider(idProvider) { }

	~IdMap() { Clear(); }

	bool Get(uint32_t id, T &value)
	{
		std::lock_guard<std::recursive_mutex> lock(m_elementsMutex);

		auto pair = m_idsToElements.find(id);
		if (pair != m_idsToElements.end())
		{
			value = pair->second;
			return true;
		}

		return false;
	}

	bool GetId(T element, uint32_t &id)
	{
		std::lock_guard<std::recursive_mutex> lock(m_elementsMutex);

		auto pair = m_elementsToIds.find(element);
		if (pair != m_elementsToIds.end())
		{
			id = pair->second;
			return true;
		}

		return false;
	}

	bool AddOrGetExisting(T element, uint32_t &id)
	{
		std::lock_guard<std::recursive_mutex> lock(m_elementsMutex);

		if (GetId(element, id))
		{
			return false;
		}

		id = m_idProvider->GetNext();
		m_idsToElements.emplace(id, element);
		m_elementsToIds.emplace(element, id);

		return true;
	}

	bool Remove(uint32_t id)
	{
		std::lock_guard<std::recursive_mutex> lock(m_elementsMutex);

		auto pair = m_idsToElements.find(id);
		if (pair != m_idsToElements.end())
		{
			m_idsToElements.erase(id);
			m_elementsToIds.erase(pair->second);
			return true;
		}

		return false;
	}

	bool Remove(T element)
	{
		std::lock_guard<std::recursive_mutex> lock(m_elementsMutex);

		uint32_t id;
		if (GetId(element), id)
		{
			return Remove(id);
		}

		return false;
	}

	void Clear()
	{
		std::lock_guard<std::recursive_mutex> lock(m_elementsMutex);

		m_idsToElements.clear();
		m_elementsToIds.clear();
	}
};

}
