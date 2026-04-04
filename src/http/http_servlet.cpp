#include "http_servlet.h"
#include <fnmatch.h>

namespace sylar{
namespace http{


/*--------------------------------------------------  FunctionServlet  ------------------------------------------------------*/
FunctionServlet::FunctionServlet(callback cb)
    : Servlet("FunctionServlet")
    , m_cb(cb)
{}

int32_t FunctionServlet::handle(HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session)
{
    return m_cb(req, rsp, session);
}


/*--------------------------------------------------  RouteServlet  ------------------------------------------------------*/
void ServletDispatch::addRoute(HttpMethod method, const std::string &path, Servlet::ptr servlet) {
    RWMutexType::WriteLock lock(m_mutex);
    RouteKey key{method, path};
    m_routes[key] = servlet;
}

void ServletDispatch::addRoute(HttpMethod method, const std::string &path, FunctionServlet::ptr servlet)
{
    RWMutexType::WriteLock lock(m_mutex);
    RouteKey key{method, path};
    m_routes[key] = servlet;
}

void ServletDispatch::addRouteCallback(HttpMethod method, const std::string &path, FunctionServlet::callback callback)
{
    RWMutexType::WriteLock lock(m_mutex);
    RouteKey key{method, path};
    m_routecbs[key] = callback;
}

void ServletDispatch::addRegexRoute(HttpMethod method, const std::string &path, FunctionServlet::ptr servlet)
{
    RWMutexType::WriteLock lock(m_mutex);
    std::regex pathRegex = convertToRegex(path);
    m_regexs.emplace_back(method,  pathRegex, servlet);
}   

void ServletDispatch::addRegexRouteCallback(HttpMethod method, const std::string &path, FunctionServlet::callback callback)
{
    RWMutexType::WriteLock lock(m_mutex);
    std::regex pathRegex = convertToRegex(path);
    m_regexcbs.emplace_back(method,  pathRegex, callback);
}

std::regex ServletDispatch::convertToRegex(const std::string &path)
{
    std::string regexPattern = "^" + std::regex_replace(path, std::regex(R"(/:([^/]+))"), R"(/([^/]+))") + "$";
    return std::regex(regexPattern);
}

void ServletDispatch::extractPathParameters(const std::smatch &match, HttpRequest::ptr req)
{
    for (size_t i = 0; i < match.size(); ++i)
    {
        req->setParam("param" + std::to_string(i), match[i].str());
    }
}


/*--------------------------------------------------  ServletDispatch  ------------------------------------------------------*/
ServletDispatch::ServletDispatch()
    : Servlet("ServletDispatch")
{
    m_default.reset(new NotFoundServlet("sylar/1.0"));
}

int32_t ServletDispatch::handle(HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session)
{
    auto slt = getMatchedRoute(req);
    // 这说明没有注册对应的Route
    if (!slt)
    {
        slt = getMatchedServlet(req->getPath());
    }
    // 执行请求注册好的Servlet
    if (slt)
    {
        slt->handle(req, rsp, session);
    }
    return 0;
}

void ServletDispatch::addServlet(const std::string& url, Servlet::ptr slt)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[url] = slt;
}

void ServletDispatch::addServlet(const std::string& url, FunctionServlet::callback cb)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[url].reset(new FunctionServlet(cb));
}

void ServletDispatch::addGlobServlet(const std::string& url, Servlet::ptr slt)
{
    RWMutexType::WriteLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it)
    {
        if (it->first == url)
        {
            m_globs.erase(it);
            break;
        }
    }
    m_globs.push_back(std::make_pair(url, slt));
}

void ServletDispatch::addGlobServlet(const std::string& url, FunctionServlet::callback cb)
{
    return addGlobServlet(url, FunctionServlet::ptr(new FunctionServlet(cb)));
}

void ServletDispatch::deleteServlet(const std::string& url)
{
    RWMutexType::WriteLock lock(m_mutex);
    m_datas.erase(url);
}

void ServletDispatch::delGlobServlet(const std::string& url)
{
    RWMutexType::WriteLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it)
    {
        if (it->first == url)
        {
            m_globs.erase(it);
            break;
        }
    }
}

Servlet::ptr ServletDispatch::getServlet(const std::string& url)
{
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_datas.find(url);
    return it == m_datas.end() ? nullptr :it->second;
}

Servlet::ptr ServletDispatch::getGlobServlet(const std::string& url)
{
    RWMutexType::ReadLock lock(m_mutex);
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it)
    {
        if (it->first == url)
        {
            return it->second;
        }
    }
    return nullptr;
}

Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& url)
{
    RWMutexType::ReadLock lock(m_mutex);
    auto mit = m_datas.find(url);
    if (mit != m_datas.end())
    {
        return mit->second;
    }
    for (auto it = m_globs.begin(); it != m_globs.end(); ++it)
    {
        if (!fnmatch(it->first.c_str(), url.c_str(), 0))
        {
            return it->second;
        }
    }
    return m_default;
}

Servlet::ptr ServletDispatch::getMatchedRoute(HttpRequest::ptr req) 
{
    RWMutexType::ReadLock lock(m_mutex);
    std::string path = req->getPath();
 
    // 1. 精确路由匹配（方法+路径）
    RouteKey key{req->getMethod(), path};
    auto rit = m_routes.find(key); 
    if (rit != m_routes.end()) 
    {
        return rit->second;
    }
 
    // 2. 精确路由回调（方法+路径）
    auto rcit = m_routecbs.find(key); 
    if (rcit != m_routecbs.end()) 
    {
        return std::make_shared<FunctionServlet>(rcit->second);
    }
 
    // 3. 动态路由匹配（正则+方法）
    for (const auto& route : m_regexs) 
    {
        std::smatch match;
        if (route.method  == req->getMethod() && std::regex_match(path, match, route.regex))  
        {
            extractPathParameters(match, req);
            return route.servlet; 
        }
    }
 
    // 4. 动态路由回调（正则+方法）
    for (const auto& route : m_regexcbs)
    {
        std::smatch match;
        if (route.method  == req->getMethod() && std::regex_match(path, match, route.regex))  
        {
            extractPathParameters(match, req);
            return std::make_shared<FunctionServlet>(route.cb); 
        }
    }
 
    return nullptr;
}


/*--------------------------------------------------  NotFoundDispatch  ------------------------------------------------------*/
NotFoundServlet::NotFoundServlet(const std::string& name)
    : Servlet("NotFoundServlet")
    , m_name(name) 
{
    m_content = "<html><head><title>404 Not Found"
        "</title></head><body><center><h1>404 Not Found</h1></center>"
        "<hr><center>" + name + "</center></body></html>";

}

int32_t NotFoundServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) 
{
    response->setStatus(sylar::http::HttpStatus::NOT_FOUND);
    response->setHeader("Server", "sylar/1.0.0");
    response->setHeader("Content-Type", "text/html");
    response->setBody(m_content);
    return 0;
}


}
}