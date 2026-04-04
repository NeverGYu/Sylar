#include "../../include/base/chatServer.h"
#include "../../include/servlet/chat.h"
#include "../../include/servlet/chat_create_and_send.h"
#include "../../include/servlet/chatentry.h"
#include "../../include/servlet/chathistory.h"
#include "../../include/servlet/chatlogin.h"
#include "../../include/servlet/chatlogout.h"
#include "../../include/servlet/chatregister.h"
#include "../../include/servlet/chatsend.h"
#include "../../include/servlet/chatsession.h"
#include "../../include/servlet/chatspeech.h"
#include "../../include/servlet/aimenu.h"
#include "../../include/servlet/aiuploader.h"
#include "../../include/servlet/aiuploader_send.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("chat");

ChatServer::ChatServer(bool keepalive
    , sylar::IOManager* worker
    , sylar::IOManager* accept_worker
    , bool use_ssl
    , sylar::ssl::SslContext::ptr ssl_ctx
    , bool useSession
    , bool useMiddle)
      : m_worker(worker)
      , m_accept(accept_worker)
      , m_httpserver(std::make_shared<sylar::http::HttpServer>(keepalive, m_worker, m_accept, use_ssl,ssl_ctx, useSession, useMiddle))
{
    initialize();
}

void ChatServer::initialize()
{
    try {
        const std::string mysqlHost = chatserver::envOrDefault("CHAT_SERVER_MYSQL_HOST", "tcp://127.0.0.1:3306");
        const std::string mysqlUser = chatserver::envOrDefault("CHAT_SERVER_MYSQL_USER", "root");
        const std::string mysqlPassword = chatserver::envOrDefault("CHAT_SERVER_MYSQL_PASSWORD", "root");
        const std::string mysqlDatabase = chatserver::envOrDefault("CHAT_SERVER_MYSQL_DB", "chat_server");
        const int poolSize = chatserver::envIntOrDefault("CHAT_SERVER_MYSQL_POOL_SIZE", 10);

        sylar::db::MysqlUtil::init(mysqlHost, mysqlUser, mysqlPassword, mysqlDatabase, poolSize);
        m_databaseAvailable = true;
        m_databaseError.clear();
        readDataFromMySQL();
    } catch (const std::exception& e) {
        m_databaseAvailable = false;
        m_databaseError = e.what();
        SYLAR_LOG_WARN(g_logger) << "chat_server database disabled: " << m_databaseError;
    }
    // 初始化会话
    initializeSession();
    // 初始化中间件
    initializeMiddleware();
    // // 初始化路由
    // initializeRouter();
}

void ChatServer::initializeSession()
{
    // 创建会话存储
    auto sessionStorage = std::make_unique<sylar::http::MemorySessionStorage>();
    // 创建会话管理器
    auto sessionManager = std::make_shared<sylar::http::SessionManager>(std::move(sessionStorage));
    // 设置会话管理器
    m_httpserver->setSessionManager(sessionManager);
}

void ChatServer::initializeMiddleware()
{
    // 创建中间件
    auto corsMiddleware = std::make_shared<sylar::middleware::CorsMiddleWare>();
    // 添加到httpServer
    m_httpserver->addMiddleware(corsMiddleware);
}
        
void ChatServer::initializeRouter() {
    auto dispatch = m_httpserver->getServletDispatch();
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/", std::make_shared<ChatEntry>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/entry", std::make_shared<ChatEntry>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/login", std::make_shared<ChatLogin>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/register", std::make_shared<ChatRegister>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/user/logout", std::make_shared<ChatLogout>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/chat", std::make_shared<Chat>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/chat/send", std::make_shared<ChatSend>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/chat/history", std::make_shared<ChatHistory>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/chat/send-new-session", std::make_shared<ChatCreateAndSend>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/chat/sessions", std::make_shared<ChatSessions>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/chat/tts", std::make_shared<ChatSpeech>(shared_from_this()));

    dispatch->addRoute(sylar::http::HttpMethod::GET, "/menu", std::make_shared<AiMenu>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/upload", std::make_shared<AiUpload>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/upload/send", std::make_shared<AiUploadSend>(shared_from_this()));
}

void ChatServer::requireDatabase() const {
    if (m_databaseAvailable) {
        return;
    }
    throw std::runtime_error(
        "MySQL is unavailable for chat_server. Set CHAT_SERVER_MYSQL_HOST/USER/PASSWORD/DB and start MySQL first. Last error: "
        + m_databaseError);
}


void ChatServer::packageResp(uint8_t version, sylar::http::HttpStatus status,
                    const std::string& statusMsg, bool close, const std::string& contentType,
                    int cttlen, const std::string& body, sylar::http::HttpResponse::ptr rsp)
{
    if (rsp == nullptr) 
    {
        SYLAR_LOG_ERROR(g_logger) << "Response pointer is null";
        return;
    }

    try 
    {
        rsp->setVersion(version);
        rsp->setStatus(status);
        rsp->setReason(statusMsg);
        rsp->setClose(close);
        rsp->setHeader("content-type", contentType);
        rsp->setHeader("content-length", std::to_string(cttlen));
        rsp->setBody(body);
        
        SYLAR_LOG_INFO(g_logger) << "Response packaged successfully";
    }
    catch (const std::exception& e) 
    {
        SYLAR_LOG_ERROR(g_logger) << "Error in packageResp: " << e.what();
        // 设置一个基本的错误响应
        rsp->setStatus(sylar::http::HttpStatus::INTERNAL_SERVER_ERROR);
        rsp->setReason("Internal Server Error");
        rsp->setClose(true);
    }
}

void ChatServer::readDataFromMySQL() {
    requireDatabase();

    std::string sql = "SELECT id, username,session_id, is_user, content, ts FROM chat_message ORDER BY ts ASC, id ASC";

    sql::ResultSet* res;
    try {
        res = m_mysql.executeQuery(sql);
    }
    catch (const std::exception& e) {
        std::cerr << "MySQL query failed: " << e.what() << std::endl;
        return;
    }

    while (res->next()) {
        long long user_id = 0;
        std::string session_id ;  
        std::string username, content;
        long long ts = 0;
        int is_user = 1;

        try {
            user_id    = res->getInt64("id");       
            session_id = res->getString("session_id");  
            username   = res->getString("username");
            content    = res->getString("content");
            ts         = res->getInt64("ts");
            is_user    = res->getInt("is_user");
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to read row: " << e.what() << std::endl;
            continue; 
        }

        auto& userSessions = chatInformation[user_id];

        std::shared_ptr<AIHelper> helper;
        auto itSession = userSessions.find(session_id);
        if (itSession == userSessions.end()) {
            helper = std::make_shared<AIHelper>();
            userSessions[session_id] = helper;
			sessionsIdsMap[user_id].push_back(session_id);
        } else {
            helper = itSession->second;
        }

        helper->restoreMessage(content, ts);
    }

    std::cout << "readDataFromMySQL finished" << std::endl;
}
