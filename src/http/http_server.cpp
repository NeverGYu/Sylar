#include "http_server.h"
#include "session_manager.h"
#include "hook.h"
#include "../middleware/middleware.h"
#include "../stream/sslsocket_stream.hpp"

namespace sylar{
namespace http{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HttpServer::HttpServer(bool keepalive, sylar::IOManager* worker
                    , sylar::IOManager* accept_worker
                    , bool use_ssl
                    , sylar::ssl::SslContext::ptr ssl_ctx
                    , bool useSession
                    , bool useMiddle)
    : TcpServer(worker, accept_worker)
    , m_isKeepalive(keepalive)
    , m_useSsl(use_ssl)
    , m_ctx(ssl_ctx)
    , m_useSession(useSession)
    , m_useMiddle(useMiddle)
{
    m_dispatch.reset(new ServletDispatch);
    m_sessionManager.reset(new SessionManager(std::make_unique<MemorySessionStorage>()));
    m_middleChain.reset(new middleware::MiddleChain);
}

void HttpServer::addMiddleware(middleware::MiddleWare::ptr middleware)
{
    m_middleChain->addMiddleware(middleware);
}

void HttpServer::setSsl(bool enabled, sylar::ssl::SslContext::ptr ctx) 
{
    m_useSsl = enabled;
    m_ctx = ctx;
}

void HttpServer::handleClient(Socket::ptr client)
{
    SYLAR_LOG_INFO(g_logger) << "hook=" << sylar::is_hook_enable();

    SYLAR_LOG_DEBUG(g_logger) << "handleClient " << *client;
    // 用于封装Socket
    SocketStream::ptr stream;
    // 判断是否使用SSL
    if (m_useSsl)
    {
        auto ssl_stream = std::make_shared<sylar::ssl::SslSocketStream>(client, m_ctx);
        if (!ssl_stream->doHandShake())
        {
            SYLAR_LOG_ERROR(g_logger) << "SSL Handshake failed with client: " << *client;
            ssl_stream->close();
            return;
        }
        stream = ssl_stream;
    }
    else
    {
        stream = std::make_shared<SocketStream>(client);
    }   
    // 根据Socket client 建立连接 accept
    HttpSession::ptr session = std::make_shared<HttpSession>(stream);
    do
    {
        // accept 用来接收用户请求
        auto req = session->recvRequest();
        // 判断是否连接请求是否为空
        if(!req) 
        {
            SYLAR_LOG_DEBUG(g_logger) << "recv http request fail, errno="
                                      << errno << " errstr=" << strerror(errno)
                                      << " cliet:" << *client << " keep_alive=" << m_isKeepalive;
            break;
        }
        // 根据Http请求创建Http响应
        HttpResponse::ptr rsp(new HttpResponse(req->getVersion(), req->isClose() || !m_isKeepalive));
        if (m_useSession) 
        {
             // 查找这个Http请求是否已经连接过，没有则创建新会话Session，有的话就加载会话
            auto s = m_sessionManager->getSession(req, rsp);
            // 将Http请求设置会话
            req->setSession(s);
            // 中间件处理--> 注意：rsp 是 throw 出来的
        }
        if (m_useMiddle)
        {
            try 
            {
                if (m_middleChain) 
                {
                    m_middleChain->processBefore(req); 
                }
            } 
            catch (sylar::http::HttpResponse::ptr res) 
            {
                // 处理中间件抛出的异常
                SYLAR_LOG_INFO(g_logger) << "Caught HttpResponse from middleware with status: " << (int)res->getStatus();
                rsp = res;
            }
            catch(const std::exception& e)
            {
                // 错误处理
                rsp->setStatus(HttpStatus::EXPECTATION_FAILED);
                rsp->setBody(e.what());
            }
        }

        // 设置Http响应
        rsp->setHeader("Server", getName());
        // 调用对应的Servlet来处理请求和设置响应
        m_dispatch->handle(req, rsp, session);
        // 响应后中间件处理（如 CORS 头）
        if (m_useMiddle && m_middleChain)
        {
            m_middleChain->processAfter(rsp);
        }
        // 向accpet对端发送Http响应
        session->sendResponse(rsp);
            
        if(!m_isKeepalive || req->isClose()) 
        {
            break;
        }
    } while (true);

    session->close();
}


}
}
