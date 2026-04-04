#include "../base/gomoku_server.h"
#include "../servlet/login.h"
#include "../servlet/entry.h"
#include "../servlet/register.h"
#include "../servlet/logout.h"
#include "../servlet/menu.h"
#include "../servlet/aigame_move.h"
#include "../servlet/aigame_start.h"
#include "../servlet/game_backend.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("gomoku");

GomokuServer::GomokuServer(bool keepalive
    , sylar::IOManager* worker
    , sylar::IOManager* accept_worker
    , bool use_ssl
    , sylar::ssl::SslContext::ptr ssl_ctx)
        : m_worker(worker)
        , m_accept(accept_worker)
        , m_httpserver(std::make_shared<sylar::http::HttpServer>(true, m_worker, m_accept, true,ssl_ctx))
{
    initialize();
}

void GomokuServer::initialize()
{
    // 初始化数据库连接池
    sylar::db::MysqlUtil::init("tcp://127.0.0.1:3306", "root", "root", "Gomoku", 10);
    // 初始化会话
    initializeSession();
    // 初始化中间件
    initializeMiddleware();
    // // 初始化路由
    // initializeRouter();
}

void GomokuServer::initializeSession()
{
    // 创建会话存储
    auto sessionStorage = std::make_unique<sylar::http::MemorySessionStorage>();
    // 创建会话管理器
    auto sessionManager = std::make_shared<sylar::http::SessionManager>(std::move(sessionStorage));
    // 设置会话管理器
    m_httpserver->setSessionManager(sessionManager);
}

void GomokuServer::initializeMiddleware()
{
    // 创建中间件
    auto corsMiddleware = std::make_shared<sylar::middleware::CorsMiddleWare>();
    // 添加到httpServer
    m_httpserver->addMiddleware(corsMiddleware);
}

void GomokuServer::initializeRouter()
{
    auto dispatch = m_httpserver->getServletDispatch();
    // 添加初始页面
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/", std::make_shared<Entry>(shared_from_this()));
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/entry", std::make_shared<Entry>(shared_from_this()));
    // 添加登录页面
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/login", std::make_shared<Login>(shared_from_this()));
    // 添加注册页面
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/register", std::make_shared<Register>(shared_from_this()));
    // 添加登陆结束页面
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/user/logout", std::make_shared<Logout>(shared_from_this()));
    // 菜单页面
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/menu", std::make_shared<Menu>(shared_from_this()));
    // 添加下棋页面
    dispatch->addRoute(sylar::http::HttpMethod::POST, "/aibot/move", std::make_shared<AiGameMove>(shared_from_this()));
    // 开始对战ai界面
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/aibot/start", std::make_shared<AiGameStart>(shared_from_this()));
    // 后台界面
    dispatch->addRoute(sylar::http::HttpMethod::GET,"/backend", std::make_shared<GameBackend>(shared_from_this()));
    // 重新开始对战ai
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/aibot/restart",std::make_shared<sylar::http::FunctionServlet>(
        std::function<int(sylar::http::HttpRequest::ptr, sylar::http::HttpResponse::ptr, sylar::http::HttpSession::ptr)>(
            [this](sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr) -> int {
                restartChessGameVsAi(req, rsp);
                return 0;
        })
    ));
    // 后台数据获取
    dispatch->addRoute(sylar::http::HttpMethod::GET, "/backend_data",std::make_shared<sylar::http::FunctionServlet>(
        std::function<int(sylar::http::HttpRequest::ptr, sylar::http::HttpResponse::ptr, sylar::http::HttpSession::ptr)>(
            [this](sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr ) -> int {
                getBackendData(req, rsp);
                return 0;
        })
    ));
}

int GomokuServer::getUserCount()
{
    std::string sql = "SELECT COUNT(*) as count FROM users";

    sql::ResultSet* res = m_mysql.executeQuery(sql);
    if (res->next())
    {
        return res->getInt("count");
    }
     return 0;
}


int GomokuServer::restartChessGameVsAi(const sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp)
{
    // 解析请求体
    auto session = m_httpserver->getSessionManager()->getSession(req, rsp);
    if (session->getValue("isLoggedIn") != "true")
    {
        // 用户未登录，返回未授权错误
        nlohmann::json errorResp;
        errorResp["status"] = "error";
        errorResp["message"] = "Unauthorized";
        std::string errorBody = errorResp.dump(4);

        packageResp(req->getVersion(), sylar::http::HttpStatus::UNAUTHORIZED,
                    "Unauthorized", true, "application/json", errorBody.size(),
                    errorBody, rsp);
        return -1;
    }

    int userId = std::stoi(session->getValue("userId"));
    {
        // 重新开始ai对战
        RWMutexType::WriteLock lock(m_mutex_ai_game);
        if (m_aiGames.find(userId) != m_aiGames.end())
        {
            m_aiGames.erase(userId);
        }
        m_aiGames[userId] = std::make_shared<AiGame>(userId);
    }

    nlohmann::json successResp;
    successResp["status"] = "ok";
    successResp["message"] = "restart successful";
    successResp["userId"] = userId;
    std::string successBody = successResp.dump(4);
    packageResp(req->getVersion(),sylar::http::HttpStatus::OK, "OK", false, 
        "application/json", successBody.size(), successBody, rsp);
    return 0;
}


int GomokuServer::getBackendData(const sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp)
{
    try 
    {
        // 获取数据
        int curOnline = getCurOnline();
        SYLAR_LOG_INFO(g_logger) << "当前在线人数: " << curOnline;
        
        int maxOnline = getMaxOnline();
        SYLAR_LOG_INFO(g_logger) << "历史最高在线人数: " << maxOnline;
        
        int totalUser = getUserCount();
        SYLAR_LOG_INFO(g_logger) << "已注册用户总数: " << totalUser;

        // 构造 JSON 响应
        nlohmann::json respBody;
        respBody = {
            {"curOnline", curOnline},
            {"maxOnline", maxOnline},
            {"totalUser", totalUser}
        };

        // 转换为字符串
        std::string responseStr = respBody.dump(4);
        
        // 设置响应
        rsp->setStatus(sylar::http::HttpStatus::OK);
        rsp->setHeader("content-type", "application/json");
        rsp->setBody(responseStr);
        rsp->setHeader("content-length", std::to_string(responseStr.size()));
        rsp->setClose(false);

        SYLAR_LOG_INFO(g_logger) << "Backend data response prepared successfully";
    }
    catch (const std::exception& e) 
    {
        SYLAR_LOG_ERROR(g_logger) << "Error in getBackendData: " << e.what();
        
        // 错误响应
        nlohmann::json errorBody = {
            {"error", "Internal Server Error"},
            {"message", e.what()}
        };
        
        std::string errorStr = errorBody.dump();
        rsp->setStatus(sylar::http::HttpStatus::INTERNAL_SERVER_ERROR);
        rsp->setReason("Internal Server Error");
        rsp->setHeader("content-type", "application/json");
        rsp->setHeader("content-length", std::to_string(errorStr.size()));
        rsp->setBody(errorStr);
        rsp->setClose(true);
    }

    return 0;
}

void GomokuServer::packageResp(uint8_t version, sylar::http::HttpStatus status,
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