#include "session.h"
#include "session_manager.h"

namespace sylar{
namespace http{

/*-------------------------------  Session  ----------------------------------*/

Session::Session(const std::string& sessionId, SessionManager* sessionManager, int maxAge)
    : m_sessionId(sessionId)
    , maxAge(maxAge)
    , m_sessionManager(sessionManager)
{
    refresh(); // 初始化时设置过期时间
}

std::string Session::getSessionId()
{
    RWMutexType::ReadLock lock(m_mutex);
    return m_sessionId;
}

void Session::setSessionId(std::string &val)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_sessionId = val;
}


SessionManager* Session::getSessionManager()
{
    RWMutexType::ReadLock lock(m_mutex);
    return m_sessionManager;
}


void Session::setSessionManager(SessionManager* val)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_sessionManager = val;
}

void Session::setValue(const std::string &key, const std::string &value)
{   
    {
        RWMutexType::WriteLock lock(m_mutex);
        m_datas[key] = value;
    }

    // 如果设置了manager，自动保存更改
    if (m_sessionManager)
    {
        m_sessionManager->updateSession(shared_from_this());
    }
}


std::string Session::getValue(const std::string &key)
{
    RWMutexType::ReadLock lock(m_mutex);
    return m_datas[key];
}


void Session::remove(const std::string &key)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_datas.erase(key);
}

void Session::clear()
{
    RWMutexType::ReadLock lock(m_mutex);
    m_datas.clear();
}


}
}