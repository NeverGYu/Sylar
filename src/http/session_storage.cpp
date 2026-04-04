#include "../http/session/session_storage.h"

namespace sylar{
namespace http{

/*-----------------------------------  MemorySessionStorage  -----------------------------------------------*/
void MemorySessionStorage::save(Session::ptr session)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_sessions[session->getSessionId()] = session;
}

Session::ptr MemorySessionStorage::load(const std::string& sessionId)
{
    RWMutexType::WriteLock lock(m_mutex);
    auto it  = m_sessions.find(sessionId);
    if (it != m_sessions.end())
    {
        if (!it->second->isExpired())
        {
            return it->second;
        }
        else
        {
            m_sessions.erase(it);
        }
    }
    return nullptr;
}

void MemorySessionStorage::remove(const std::string& sessionId)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_sessions.erase(sessionId);
}


}
}