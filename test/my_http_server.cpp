#include "sylar.h"
#include "log.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

static sylar::IOManager::ptr worker;

void run()
 {
    // g_logger->setLoggerLevel(sylar::LogLevel::DEBUG);

    sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("0.0.0.0:8020");
    if(!addr) 
    {
        SYLAR_LOG_ERROR(g_logger) << "get address error";
        return;
    }

    sylar::http::HttpServer::ptr http_server(
        new sylar::http::HttpServer(true, sylar::IOManager::GetThis(), sylar::IOManager::GetThis()
            , false, nullptr, false, false));
    
    auto sd = http_server->getServletDispatch();
    sd->addServlet("/sylar/xx", [](sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session)
                   { rsp->setHeader("Content-Type", "text/plain; charset=utf-8");
                     rsp->setBody("OK\n");
                     return 0; });       

    while(!http_server->bind(addr)) 
    {
        SYLAR_LOG_ERROR(g_logger) << "bind " << *addr << " fail";
        sleep(1);
    }

    http_server->start();
}

int main(int argc, char** argv) 
{
    auto root_logger = SYLAR_LOG_ROOT();
    root_logger->setLoggerLevel(sylar::LogLevel::FATAL);
    root_logger->clearAppenders();

    auto system_logger = SYLAR_LOG_NAME("system");
    system_logger->setLoggerLevel(sylar::LogLevel::FATAL);
    system_logger->clearAppenders();

    auto http_logger = SYLAR_LOG_NAME("http");
    http_logger->setLoggerLevel(sylar::LogLevel::FATAL);
    http_logger->clearAppenders();

    sylar::IOManager iom(3);
    iom.schedule(run);
    return 0;
}
