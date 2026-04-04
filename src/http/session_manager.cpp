#include "session_manager.h"

namespace sylar{
namespace http{

/*-------------------------------  SessionManager  ----------------------------------*/
SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage)
    : m_storage(std::move(storage))
    , m_random(std::random_device{}())
{}

Session::ptr SessionManager::getSession(const HttpRequest::ptr req,  const HttpResponse::ptr rsp)
{
    // 先确保 cookie 已解析（如果你的 HttpRequest::getCookie 依赖 initCookies）
    if (req) {
        req->initCookies();
    }

    // 优先从 cookie 里拿 sessionId
    std::string sessionId;
    if (req) {
        sessionId = req->getCookie("sessionId");
        if (sessionId.empty()) {
            // 兼容旧逻辑：从原始 Cookie 头解析
            sessionId = getSessionIdFromCookie(req);
        }
    }

    Session::ptr session;

    if (!sessionId.empty()) {
        session = m_storage->load(sessionId);
        if (session && session->isExpired()) {
            m_storage->remove(sessionId);
            session.reset();
        }
    }

    bool created = false;
    if (!session) {
        sessionId = generateSessionId();
        session = std::make_shared<Session>(sessionId, this);
        created = true;
    }

    session->setSessionManager(this);
    session->refresh();
    m_storage->save(session);

    // 仅新建时下发 Set-Cookie，避免覆盖已存在 cookie
    if (created && rsp) {
        setSessionCookie(sessionId, rsp);
    }

    return session;
}


std::string SessionManager::generateSessionId()
{
    // 生成唯一的会话标识符，确保会话的唯一性和安全性
    std::stringstream ss;
    std::uniform_int_distribution<> dist(0, 15);

    // 生成32个字符的会话ID，每个字符是一个十六进制数字
    for (int i = 0; i < 32; ++i)
    {
        ss << std::hex << dist(m_random);
    }
    return ss.str();
}

void SessionManager::destroySession(const std::string& sessionId)
{
    m_storage->remove(sessionId);
}

void SessionManager::cleanExpiredSessions()
{
    // 注意：这个实现依赖于具体的存储实现
    // 对于内存存储，可以在加载时检查是否过期
    // 对于其他存储的实现，可能需要定期清理过期会话
}

std::string SessionManager::getSessionIdFromCookie(const HttpRequest::ptr req)
{
    std::string sessionId;
    std::string cookie = req->getHeader("Cookie");

    if (!cookie.empty())
    {
        const std::string key = "sessionId=";
        size_t pos = cookie.find(key);
        
        if (pos != std::string::npos)
        {
            pos += key.size(); // 跳过"sessionId="
            size_t end = cookie.find(';', pos);
            if (end != std::string::npos)
            {
                sessionId = cookie.substr(pos, end - pos);
            }
            else
            {
                sessionId = cookie.substr(pos);
            }
        }
    }
    
    return sessionId;
}

void SessionManager::setSessionCookie(const std::string& sessionId, HttpResponse::ptr resp)
{
    // 设置会话ID到响应头中，作为Cookie
    std::string cookie = "sessionId=" + sessionId + "; Path=/; HttpOnly";
    resp->setHeader("Set-Cookie", cookie);
}

}
}
