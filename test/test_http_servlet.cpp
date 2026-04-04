#include "sylar.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test()
{
    sylar::http::HttpServer::ptr server(new sylar::http::HttpServer);
    
    sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("0.0.0.0:8020");
    
    if (!addr)
    {
        SYLAR_LOG_INFO(g_logger) << "access addr fails";
    }
    
    while (!server->bind(addr))
    {
        sleep(2);
    }
    
    auto sd = server->getServletDispatch();
    sd->addServlet("/sylar/xx", [](sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session){
        rsp->setBody(req->toString());
        return 0;
    });

    sd->addGlobServlet("/sylar/*",[](sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session){
        rsp->setBody(req->toString());
        return 0;
    });

    sd->addRoute(sylar::http::HttpMethod::GET, "/api/users", std::make_shared<sylar::http::FunctionServlet>(
        [](sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr res, sylar::http::HttpSession::ptr) {
        auto id = req->getParam("param1"); // 提取动态参数 
        res->setBody("UserDetail ID: " + id);
        return 0;
    }));

    server->start();
}


int main()
{
    sylar::IOManager iom(1);
    iom.schedule(&test);
}