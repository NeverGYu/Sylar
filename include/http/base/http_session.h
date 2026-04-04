#pragma once
#include "../stream/socket_stream.hpp"
#include "http.h"
#include <memory>
#include <string>

namespace sylar{
namespace http{

/**
 *  @brief HttpSession用来封装进行accept的socket 
 */
class HttpSession
{
public:
    using ptr = std::shared_ptr<HttpSession>;

    /**
     *  @brief 构造函数 
     */
    HttpSession(SocketStream::ptr stream, bool owner = true);


    /**
     *  @brief 用来接收Http请求 
     */
    HttpRequest::ptr recvRequest();

    /**
     *  @brief 用来发送Http响应
     *  @param[in] rsp HTTP响应
     *  @return >0 发送成功
     *          =0 对方关闭
     *          <0 Socket异常
     */
    int sendResponse(HttpResponse::ptr rsp);

    /**
     *  @brief 关闭套接字 
     */
    void close();
    
private:
    SocketStream::ptr m_stream;
    std::string m_recvBuffer;
};

}
}
