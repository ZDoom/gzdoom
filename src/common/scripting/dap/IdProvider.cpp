#include "IdProvider.h"

namespace DebugServer
{
uint32_t IdProvider::GetNext()
{
	std::lock_guard<std::mutex> lock(m_idMutex);
	return m_currentId++;
}
}
