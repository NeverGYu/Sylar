#pragma once

#include <memory>
#include <string>
#include <map>
#include <chrono>
#include <random>
#include <unordered_map>
#include "util.h"
#include "mutex.h"
#include "http.h"

namespace sylar{
namespace http{

class SessionManager;

/**
 *  @brief Session类表示用户会话，它保存并会话数据并管理会话过期 
 */
class Session : public std::enable_shared_from_this<Session>
{
public:
    using ptr = std::shared_ptr<Session>;
    using RWMutexType = RWMutex;

    /**
     *  @brief 构造函数
     */
    Session(const std::string& sessionId, SessionManager* sessionManager, int maxAge = 3600);

    /**
     *  @brief 析构函数 
     */
    ~Session() {}

    /**
     *  @brief 获取会话ID 
     */
    std::string getSessionId();

    /**
     *  @brief 设置会话ID 
     */
    void setSessionId(std::string& val);
    /**
     *  @brief 判断会话是否超时 
     */
    bool isExpired() const { return GetCurrentMS() > m_expiredTime; }

    /**
     *   @brief 刷新超时时间
     */
    void refresh() { m_expiredTime = GetCurrentMS() + maxAge * 1000ULL; }

    /**
     *  @brief 获得当前的会话管理 
     */
    SessionManager* getSessionManager();

    /**
     *  @brief 设置当前的会话管理 
     */
    void setSessionManager(SessionManager* val);

    /**
     *  @brief 存储会话数据 
     */
    void setValue(const std::string& key, const std::string& value);

    /**
     *  @brief 获取指定内容的会话数据 
     */
    std::string getValue(const std::string& key);

    /**
     *  @brief 删除指定内容的会话数据 
     */
    void remove(const std::string& key);

    /**
     *  @brief 删除会话 
     */
    void clear();

private:
    std::string m_sessionId;                                // 会话ID
    std::unordered_map<std::string, std::string> m_datas;   // 会话数据
    uint64_t m_expiredTime;                                 // 过期时间  
    uint64_t maxAge;                                        // 最大存活时间
    SessionManager* m_sessionManager;                       // 会话管理
    RWMutexType m_mutex;                                    // 读写锁
};




}
}