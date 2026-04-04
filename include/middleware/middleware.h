#pragma once 

#include "../http/base/http.h"
#include "../base/mutex.h"
#include <memory>
#include <vector>

namespace sylar{
namespace middleware{

/**
 *  @class 抽象基类 
 */
class MiddleWare{
public:
    using ptr = std::shared_ptr<MiddleWare>;
    
    /**
     *  @brief 虚析构函数 
     */
    virtual ~MiddleWare() {};

    /**
     *  @brief 请求前处理
     *  @param[in] req Http请求 
     */
    virtual void before(sylar::http::HttpRequest::ptr req) = 0;

    /**
     *  @brief 响应后处理
     *  @param[in] rsp Http响应 
     */
    virtual void after(sylar::http::HttpResponse::ptr rsp) = 0;

    /**
     *  @brief 设置下一个中间件 
     *  @param[in] next 中间件
     */
    void setNext(MiddleWare::ptr next) { m_nextMiddleware = next; }
private:
    MiddleWare::ptr m_nextMiddleware;
};


/**
 *  @class 中间件链 
 */
class MiddleChain
{
public:
    using ptr = std::shared_ptr<MiddleChain>;

    /**
     *  @brief 构造函数 
     */
    MiddleChain() = default;
    
    /**
     *  @brief 添加一个中间件
     *  @param[in] middleware 
     */
    void addMiddleware(MiddleWare::ptr middle);

    /**
     *  @brief 请求前处理
     *  @param[in] req Http请求 
     */
    void processBefore(sylar::http::HttpRequest::ptr req);

    /**
     *  @brief 响应后处理
     *  @param[in] rsp Http响应 
     */
    void processAfter(sylar::http::HttpResponse::ptr rsp);

private:
    std::vector<MiddleWare::ptr> m_middlewares;
};





}
}
