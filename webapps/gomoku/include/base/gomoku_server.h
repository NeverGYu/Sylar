#pragma once

#include "../../include/base/sylar.h"
#include "../../include/db/mysql.h"
#include "../base/ai_game.h"
#include <unordered_map>
#include <memory>
#include <atomic>

static sylar::ConfigVar<int>::ptr g_max_aibot = 
    sylar::Config::Lookup("gomoku.aibot", "AI 数量", 4096);

class Login;
class Logout;
class AiGameMove;
class Menu;

class GomokuServer : public std::enable_shared_from_this<GomokuServer>
{
public:
    using RWMutexType = sylar::RWMutex;
    using ptr = std::shared_ptr<GomokuServer>;

    /**
     *  @brief 构造函数
     */
    GomokuServer(bool keepalive = true
        , sylar::IOManager* worker = sylar::IOManager::GetThis()
        , sylar::IOManager* accept_worker = sylar::IOManager::GetThis()
        , bool use_ssl = true
        , sylar::ssl::SslContext::ptr ssl_ctx = nullptr);

    /**
     *  @brief 绑定IP和端口 
     */
    bool bind(sylar::Address::ptr address) { return m_httpserver->bind(address); }

    
    /**
     *  @brief 启动服务器 
     */
    void start() { m_httpserver->start(); }
    
    /**
     *  @brief 初始路由 
     */
    void initializeRouter();

private:
    /**
     *  @brief 游戏类型 
     */
    enum Gametype
    {
        NO_GAME = 0,
        MAN_VS_AI = 1,
        MAN_VS_MAN = 2,
    };

private:
    friend class Login; 
    friend class Logout;
    friend class Menu;
    friend class AiGameMove;
    friend class AiGameStart;
    friend class GameBackend;

private:
    /**
     *  @brief 初始化相关组件 
     */
    void initialize();

    /**
     *  @brief 初始会话 
     */
    void initializeSession();

    /**
     *  @brief 初始中间件 
     */
    void initializeMiddleware();

    /**
     *  @brief 更新在线人数 
     */
    void updateMaxOnline(int online){ m_maxOnline = std::max(m_maxOnline.load(), online); }

    /**
      *  @brief 获取历史最高在线人数
      */ 
    int getMaxOnline() const { return m_maxOnline.load(); }

    /**
     *  @brief 获取当前在线人数
     */ 
    int getCurOnline() const { return m_online_users.size(); }

    /**
     *  @brief 获取用户总数
     */ 
    int getUserCount();

    /**
     *  @brief 重启棋局 
     */
    int restartChessGameVsAi(const sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp);

    /**
     *  @brief 获得后台数据 
     */
    int getBackendData(const sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp);

    /**
     *  @brief 打包HttpResponse，准备发送给客户端
     */
    void packageResp(uint8_t version, sylar::http::HttpStatus status,
                    const std::string& statusMsg, bool close, const std::string& contentType,
                    int cttlen, const std::string& body, sylar::http::HttpResponse::ptr rsp);
private:
    sylar::IOManager* m_worker;                         // 工作线程
    sylar::IOManager* m_accept;                         // 接收请求的连接线程
    sylar::http::HttpServer::ptr m_httpserver;          // http服务器
    sylar::db::MysqlUtil m_mysql;                       // mysql数据库连接池
    std::unordered_map<int, AiGame::ptr> m_aiGames;     // 用户ID到AI五子棋
    RWMutexType m_mutex_ai_game;                        // 游戏读写锁
    std::unordered_map<int, bool> m_online_users;       // 用户是否在线
    RWMutexType m_mutex_for_online_users;               // 在线用户读写锁
    std::atomic<int> m_maxOnline;                       // 最高在线人数
};