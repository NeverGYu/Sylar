#pragma once

#include <memory>
#include <string>
#include <list>
#include "http.h"
#include "../base/url.hpp"
#include "../base/thread.h"
#include "../stream/socket_stream.hpp"
#include "../http/base/http_parser.h"

namespace sylar{
namespace http{

/**
 *  @brief Http响应结果 
 */
struct HttpResult
{
public:
    using ptr = std::shared_ptr<HttpResult>;

    enum class Error {
        OK = 0,                         // 正常
        INVALID_URL = 1,                // 非法URL
        INVALID_HOST = 2,               // 无法解析HOST
        CONNECT_FAIL = 3,               // 连接失败
        SEND_CLOSE_BY_PEER = 4,         // 连接被对端关闭
        SEND_SOCKET_ERROR = 5,          // 发送请求产生Socket错误
        TIMEOUT = 6,                    // 超时
        CREATE_SOCKET_ERROR = 7,        // 创建Socket失败
        POOL_GET_CONNECTION = 8,        // 从连接池中取连接失败
        POOL_INVALID_CONNECTION = 9,    // 无效的连接
    };

     /**
     * @brief 构造函数
     * @param[in] _result 错误码
     * @param[in] _response HTTP响应结构体
     * @param[in] _error 错误描述
     */
    HttpResult(int result, HttpResponse::ptr rsp, const std::string& error)
        : result_(result)
        , rsp_(rsp)
        , error_(error)
    {}
    
    /**
     *  @brief 转字符串 
     */
    std::string toString() const;

public:
    int result_;                 // 错误码
    HttpResponse::ptr rsp_;      // http响应结果
    std::string error_;          // 错误描述
};

class HttpConnectionPool;
/**
 *  @brief Http客户端类 
 */
class HttpConnection : public SocketStream
{
    friend class HttpConnectionPool;
public:
    using ptr = std::shared_ptr<HttpConnection>;

    /**
     *  @brief 发送Http的Get请求
     *  @param[in] url 请求的url
     *  @param[in] timeout_ms 超时时间
     *  @param[in] headers Http请求头部参数
     *  @param[in] body 请求消息体
     *  @return 返回Http结果结构体
     */
    static HttpResult::ptr DoGet(const std::string& url, uint64_t timeout_ms, const std::map<std::string,std::string>& headers = {}, const std::string& body = "");

    /**
     *  @brief 发送Http的Get请求
     *  @param[in] url 请求的url
     *  @param[in] timeout_ms 超时时间
     *  @param[in] headers Http请求头部参数
     *  @param[in] body 请求消息体
     *  @return 返回Http结果结构体
     */
    static HttpResult::ptr DoGet(Url::ptr url, uint64_t timeout_ms, const std::map<std::string,std::string>& headers = {}, const std::string& body = "");

    /**
     *  @brief 发送Http的Post请求
     *  @param[in] url 请求的url
     *  @param[in] timeout_ms 超时时间
     *  @param[in] headers Http请求头部参数
     *  @param[in] body 请求消息体
     *  @return 返回Http结果结构体
     */
    static HttpResult::ptr DoPost(const std::string& url, uint64_t timeout_ms, const std::map<std::string,std::string>& headers = {}, const std::string& body = "");

    /**
     *  @brief 发送Http的Post请求
     *  @param[in] url 请求的url
     *  @param[in] timeout_ms 超时时间
     *  @param[in] headers Http请求头部参数
     *  @param[in] body 请求消息体
     *  @return 返回Http结果结构体
     */
    static HttpResult::ptr DoPost(Url::ptr url, uint64_t timeout_ms, const std::map<std::string,std::string>& headers = {}, const std::string& body = "");

    /**
     *  @brief 发送Http请求
     *  @param[in] method 请求方法
     *  @param[in] url 请求的url
     *  @param[in] timeout_ms 超时时间
     *  @param[in] headers Http请求头部参数
     *  @param[in] body 请求消息体
     *  @return 返回Http结果结构体
     */
    static HttpResult::ptr DoRequest(HttpMethod method, const std::string& url, uint64_t timeout_ms, const std::map<std::string,std::string>& headers = {}, const std::string& body = "");

    /**
     *  @brief 发送Http请求
     *  @param[in] method 请求方法
     *  @param[in] url 请求的url
     *  @param[in] timeout_ms 超时时间
     *  @param[in] headers Http请求头部参数
     *  @param[in] body 请求消息体
     *  @return 返回Http结果结构体
     */
    static HttpResult::ptr DoRequest(HttpMethod method, Url::ptr url, uint64_t timeout_ms, const std::map<std::string,std::string>& headers = {}, const std::string& body = "");

    /**
     * @brief 发送HTTP请求
     * @param[in] req 请求结构体
     * @param[in] uri URI结构体
     * @param[in] timeout_ms 超时时间(毫秒)
     * @return 返回HTTP结果结构体
     */
    static HttpResult::ptr DoRequest(HttpRequest::ptr req, Url::ptr url, uint64_t timeout_ms);

    /**
     *  @brief 构造函数
     *  @param[in] sock Socket类
     *  @param[in] owner 是否掌握拥有权 
     */
    HttpConnection(Socket::ptr sock, bool owner = true);

    /**
     *  @brief 析构函数 
     */
    ~HttpConnection();

    /**
     *  @brief 接收Http响应 
     */
    HttpResponse::ptr recvResponse();

    /**
     *  @brief 发送Http请求 
     */
    int sendRequest(HttpRequest::ptr req);

private:
    uint64_t m_createTime = 0;      // 连接创建时间
    uint64_t m_request = 0;         // 连接使用的次数，仅在连接池的情况下使用
};


/**
 *  @brief Http连接池 
 */
class HttpConnectionPool
{
public:
    using ptr = std::shared_ptr<HttpConnectionPool>;
    using MutexType = Mutex;

    /**
     * @brief 构建HTTP请求池
     * @param[in] host 请求头中的Host字段默认值
     * @param[in] vhost 请求头中的Host字段默认值，vhost存在时优先使用vhost
     * @param[in] port 端口
     * @param[in] max_size 暂未使用
     * @param[in] max_alive_time 单个连接的最大存活时间
     * @param[in] max_request 单个连接可复用的最大次数
     */
    HttpConnectionPool(const std::string& host
        ,const std::string& vhost
        ,uint32_t port
        ,uint32_t max_size
        ,uint32_t max_alive_time
        ,uint32_t max_request);

     /**
     * @brief 从请求池中获取一个连接
     * @note 如果没有可用的连接，则会新建一个连接并加入到池，
     *       每次从池中取连接时，都会刷新一遍连接池，把超时的和已达到复用次数上限的连接删除
     */
    HttpConnection::ptr getConnection();


    /**
     * @brief 发送HTTP的GET请求
     * @param[in] url 请求的url
     * @param[in] timeout_ms 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::ptr doGet(const std::string& url
                          , uint64_t timeout_ms
                          , const std::map<std::string, std::string>& headers = {}
                          , const std::string& body = "");

    /**
     * @brief 发送HTTP的GET请求
     * @param[in] uri URI结构体
     * @param[in] timeout_ms 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::ptr doGet(Url::ptr url
                           , uint64_t timeout_ms
                           , const std::map<std::string, std::string>& headers = {}
                           , const std::string& body = "");

    /**
     * @brief 发送HTTP的POST请求
     * @param[in] url 请求的url
     * @param[in] timeout_ms 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::ptr doPost(const std::string& url
                           , uint64_t timeout_ms
                           , const std::map<std::string, std::string>& headers = {}
                           , const std::string& body = "");

    /**
     * @brief 发送HTTP的POST请求
     * @param[in] uri URI结构体
     * @param[in] timeout_ms 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::ptr doPost(Url::ptr url
                           , uint64_t timeout_ms
                           , const std::map<std::string, std::string>& headers = {}
                           , const std::string& body = "");

    /**
     * @brief 发送HTTP请求
     * @param[in] method 请求类型
     * @param[in] uri 请求的url
     * @param[in] timeout_ms 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::ptr doRequest(HttpMethod method
                            , const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    /**
     * @brief 发送HTTP请求
     * @param[in] method 请求类型
     * @param[in] uri URI结构体
     * @param[in] timeout_ms 超时时间(毫秒)
     * @param[in] headers HTTP请求头部参数
     * @param[in] body 请求消息体
     * @return 返回HTTP结果结构体
     */
    HttpResult::ptr doRequest(HttpMethod method
                            , Url::ptr url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    /**
     * @brief 发送HTTP请求
     * @param[in] req 请求结构体
     * @param[in] timeout_ms 超时时间(毫秒)
     * @return 返回HTTP结果结构体
     */
    HttpResult::ptr doRequest(HttpRequest::ptr req, uint64_t timeout_ms);

private:
    static void ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool);

private:
    std::string m_host;                 // host字段默认值
    std::string m_vhost;                // 优先级高于m_host
    uint32_t m_port;                    // 端口号
    uint32_t m_maxSize;                 // 最大连接数
    uint32_t m_maxAlivetime;            // 单个连接的最大存活时间
    uint32_t m_maxRequest;              // 单个连接的最大复用次数
    MutexType m_mutex;                  //  互斥锁
    std::list<HttpConnection*> m_conns; //连接池
    std::atomic<int32_t> m_total = {0}; // 当前连接池的可用连接数量
};

}
}