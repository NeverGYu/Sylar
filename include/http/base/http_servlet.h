#pragma once

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <regex>
#include "http.h"
#include "http_session.h"
#include "../base/mutex.h"

namespace sylar{
namespace http{

/**
 *  @brief Servlet类封装 
 */
class Servlet
{
public:
    using ptr = std::shared_ptr<Servlet>;
    /**
     *  @brief 构造函数
     *  @param[in] name 
     */
    Servlet(const std::string& name)
        : m_name(name)
    {}

    /**
     *  @brief 析构函数 
     */
    virtual ~Servlet() {}

    /**
     *  @brief 处理请求
     *  @param[in] req Http请求结构体
     *  @param[in] rsp Http响应结构体
     *  @param[in] session Http连接 
     */
    virtual int32_t handle(HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session) = 0;

    /**
     *  @brief 返回当前Servlet的名称 
     */
    std::string getName() const { return m_name; }
protected:
    std::string m_name;
};

/**
 *  @brief 函数式Servlet  
 */
class FunctionServlet : public Servlet
{
public:
    using ptr = std::shared_ptr<FunctionServlet>;
    using callback = std::function<int32_t(HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session)>;

    /**
     *  @brief 构造函数 
     */
    FunctionServlet(callback cb);

    /**
     *  @brief 对应的处理方法
     */
    virtual int32_t handle(HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session) override;
private:
    callback m_cb;
};


/**
 *  @brief Servlet 分发器
 */
class ServletDispatch : public Servlet
{
public:
    using ptr = std::shared_ptr<ServletDispatch>;
    using RWMutexType = RWMutex;

public:

    /**
     *  @brief 路由键（请求方法 + URI）
     */ 
    struct RouteKey
    {
        HttpMethod method;
        std::string path;

        bool operator==(const RouteKey &other) const
        {
            return method == other.method && path == other.path;
        }
    };

    /**
     *   @brief 为 RouteKey 定义哈希函数
     */ 
    struct RouteKeyHash
    {
        size_t operator()(const RouteKey &key) const
        {
            size_t methodHash = std::hash<int>{}(static_cast<int>(key.method));
            size_t pathHash = std::hash<std::string>{}(key.path);
            return methodHash * 31 + pathHash;
        }
    };
    
    /**
     *  @brief 动态路由回调函数 
     */
    struct RouteCallbackObj
    {
        HttpMethod method;
        std::regex regex;
        FunctionServlet::callback cb;

        RouteCallbackObj(HttpMethod m, std::regex r, FunctionServlet::callback c)
            : method(m), regex(r), cb(c)
        {}
    };

    /**
     *  @brief 动态路由处理对象 
     */
    struct RouteServletObj
    {
        HttpMethod method;
        std::regex regex;
        Servlet::ptr servlet;

        RouteServletObj(HttpMethod m, std::regex r, Servlet::ptr s)
            : method(m), regex(r), servlet(s)
        {}
    };

public:
    /**
     *  @brief 添加路由（精确匹配） 
     */
    void addRoute(HttpMethod method, const std::string &path, Servlet::ptr servlet);

    /**
     *  @brief 添加路由（精确匹配） 
     */
    void addRoute(HttpMethod method, const std::string& path, FunctionServlet::ptr servlet);

    /**
     *  @brief 添加路由回调（精确匹配）
     */
    void addRouteCallback(HttpMethod method, const std::string& path, FunctionServlet::callback callback);

    /**
     *  @brief 添加动态路由（正则匹配）
     */
    void addRegexRoute(HttpMethod method, const std::string& path, FunctionServlet::ptr servlet);

    /**
     *  @brief 添加动态路由回调（正则匹配）
     */
    void addRegexRouteCallback(HttpMethod method, const std::string& path, FunctionServlet::callback callback);

public:

    /**
     *  @brief 构造函数 
     */
    ServletDispatch();

    /**
     *  @brief url对应的处理方法 
     */
    virtual int32_t handle(HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session) override;

    /**
     *  @brief 添加一个Servlet
     *  @param[in] url 资源存放的地址
     *  @param[in] slt 该资源对应的Servlet
     */
    void addServlet(const std::string& url, Servlet::ptr slt);

    /**
     *  @brief 添加一个Servlet
     *  @param[in] url 资源存放的地址
     *  @param[in] cb 回调函数
     */
    void addServlet(const std::string& url, FunctionServlet::callback cb);

    /**
     * @brief 添加模糊匹配servlet
     * @param[in] uri uri 模糊匹配 /sylar_*
     * @param[in] slt servlet
     */
    void addGlobServlet(const std::string& url, Servlet::ptr slt);

    /**
     * @brief 添加模糊匹配servlet
     * @param[in] uri uri 模糊匹配 /sylar_*
     * @param[in] cb FunctionServlet回调函数
     */
    void addGlobServlet(const std::string& url, FunctionServlet::callback cb);

    /**
     *  @brief 删除一个Servlet 
     */
    void deleteServlet(const std::string& url);

    /**
     * @brief 删除模糊匹配servlet
     */
    void delGlobServlet(const std::string& url);

    /**
     *  @brief 返回默认的Servlet 
     */
    Servlet::ptr getDefault() const { return m_default; }

    /**
     *  @brief 设置默认的Servlet 
     */
    void setDefault(Servlet::ptr v)  { m_default = v; }

    /**
     *  @brief 通过url获取servlet
     *  @param[in] url 
     */
    Servlet::ptr getServlet(const std::string& url);

    /**
     *  @brief 通过url获取模糊匹配Servlet
     *  @param[in] url 
     */
    Servlet::ptr getGlobServlet(const std::string& url);

     /**
     * @brief 通过uri获取servlet
     * @param[in] uri uri
     * @return 优先精准匹配,其次模糊匹配,最后返回默认
     */
    Servlet::ptr getMatchedServlet(const std::string& url);

    /**
     *  @brief 通过HttpRequest来获得Route中的Servlet
     *  @param[in] req  HttpRequset
     */
    Servlet::ptr getMatchedRoute(HttpRequest::ptr req);
private:
    /**
     *  @brief 将路径模式转换为正则表达式，支持匹配任意路径参数
     */
    std::regex convertToRegex(const std::string &path);

    /**
     *  @brief  提取路径参数
     */
    void extractPathParameters(const std::smatch &match, HttpRequest::ptr req);

private:
    RWMutexType m_mutex;                                                                // 读写锁
    std::unordered_map<std::string, Servlet::ptr> m_datas;                              // 粗精准匹配 (uri --> Servlet)
    std::vector<std::pair<std::string, Servlet::ptr>> m_globs;                          // 粗模糊匹配 (uri --> Servlets)
    Servlet::ptr m_default;                                                             // 默认Servlet
    std::unordered_map<RouteKey, Servlet::ptr, RouteKeyHash> m_routes;                  // 细精确匹配 (method + url --> Servlet)
    std::unordered_map<RouteKey, FunctionServlet::callback, RouteKeyHash> m_routecbs;   // 细精确匹配回调函数
    std::vector<RouteServletObj> m_regexs;                                              // 动态路由 (uri + regex --> Servlet)
    std::vector<RouteCallbackObj> m_regexcbs;                                           // 动态路由回调 
};


/**
 * @brief NotFoundServlet(默认返回404)
 */
class NotFoundServlet : public Servlet
{
public:
    // 智能指针类型定义
    using ptr = std::shared_ptr<NotFoundServlet>;

    /**
     * @brief 构造函数
     */
    NotFoundServlet(const std::string &name);
    virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

private:
    std::string m_name;
    std::string m_content;
};




}
}