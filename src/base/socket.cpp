#include "socket.hpp"
#include "log.h"
#include "address.hpp"
#include "macro.h"
#include "fd_manager.h"
#include "hook.h"
#include "iomanager.h"
#include <netinet/tcp.h>

namespace sylar
{

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");


/*-------------------------------  创建Socket  ----------------------------------*/
Socket::ptr Socket::CreateTCP(Address::ptr addr)
{
    Socket::ptr sock(new Socket(addr->getFamily(), TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDP(Address::ptr addr)
{
    Socket::ptr sock(new Socket(addr->getFamily(), UDP, 0));
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

Socket::ptr Socket::CreateTCPSocket()
{
    Socket::ptr sock(new Socket(IPv4, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket()
{
    Socket::ptr sock(new Socket(IPv4, UDP, 0));
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}

Socket::ptr Socket::CreateTCPSocket6()
{
    Socket::ptr sock(new Socket(IPv6, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket6()
{
    Socket::ptr sock(new Socket(IPv6, UDP, 0));
    sock->newSock();
    sock->m_isConnected = true;
    return sock;
}


Socket::ptr Socket::CreateUnixTCPSocket()
{
    Socket::ptr sock(new Socket(UNIX, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket()
{
    Socket::ptr sock(new Socket(UNIX, UDP, 0));
    return sock;
}


/*------------------------------  构造函数  ------------------------------*/
Socket::Socket(int domain, int type, int protocol)
    : m_sock(-1)
    , m_family(domain)
    , m_type(type)
    , m_protocol(protocol)
    , m_isConnected(false)
{}

Socket::~Socket()
{
    close();
}

/*------------------------------- 设置socket选项  ------------------------------------*/
int64_t Socket::getSendTimeout()
{
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if (ctx)
    {
        return ctx->getTimeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeout(int64_t timeout_ms)
{
    struct timeval tv{ int(timeout_ms/ 1000), int(timeout_ms % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout()
{
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if (ctx)
    {
        return ctx->getTimeout(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeout(int64_t timeout_ms)
{
    struct timeval tv {int(timeout_ms/1000), int(timeout_ms % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void* result, socklen_t* len)
{
    int rt = getsockopt(m_sock, level, option, result, len);
    if (rt)
    {
        SYLAR_LOG_DEBUG(g_logger) << "Socket::getOptin("
                                  << " sock:" << m_sock
                                  << " level:" << level
                                  << " option: " << option
                                  << " error" << errno
                                  << " strerr" << strerror(errno);
        return false;
    }
    return true;    
}

bool Socket::setOption(int level, int option, void* result, socklen_t len)
{
    int rt = setsockopt(m_sock, level, option, result, len);
    if (rt)
    {
        SYLAR_LOG_DEBUG(g_logger) << "Socket::getOptin("
                                  << " sock:" << m_sock
                                  << " level:" << level
                                  << " option: " << option
                                  << " error" << errno
                                  << " strerr" << strerror(errno);
        return false;
    }
    return true;  
}

/*-----------------------------------  辅助函数  ------------------------------------------------------*/
bool Socket::init(int sock)
{
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock);
    if (ctx && ctx->isSocket() && !ctx->isClosed())
    {
        m_sock = sock;
        m_isConnected = true;
        initSock();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false; 
}

void Socket::initSock()
{
    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, val);
    if (m_type == SOCK_STREAM)
    {
        setOption(IPPROTO_TCP, TCP_NODELAY, val);
    }
}

void Socket::newSock()
{
    m_sock = socket(m_family, m_type, m_protocol);
    if (SYLAR_LIKELY(m_sock != -1))
    {
        initSock(); 
    }
    else
    {
        SYLAR_LOG_ERROR(g_logger) << "socket(" << m_family << ", " 
                                  << m_type << ", " 
                                  << m_protocol 
                                  << ") errno=" << errno 
                                  << " errstr=" << strerror(errno);
    }
    
}

bool Socket::isVaild() const 
{
    return m_sock != -1;
}

int Socket::getError()
{
    int error = 0;
    socklen_t len = sizeof(error);
    if(!getOption(SOL_SOCKET, SO_ERROR, &error, &len))
    {
        error = errno;
    }
    return error;
    
}

std::ostream &Socket::dump(std::ostream &os) const {
    os << "[Socket sock=" << m_sock
       << " is_connected=" << m_isConnected
       << " family=" << m_family
       << " type=" << m_type
       << " protocol=" << m_protocol;
    if (m_localAddress) 
    {
        os << " local_address=" << m_localAddress->toString();
    }
    if (m_remoteAddress) 
    {
        os << " remote_address=" << m_remoteAddress->toString();
    }
    os << "]";
    return os;
}

std::string Socket::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

std::ostream &operator<<(std::ostream &os, const Socket &sock) {
    return sock.dump(os);
}


/*-----------------------------------  针对socket的操作  -------------------------------------------*/
bool Socket::bind(Address::ptr addr)
{
    // 设置socket的源地址
    m_localAddress = addr;
    // 判断当前socket是否有效
    if (!isVaild())
    {
        newSock();
        if (SYLAR_UNLIKELY(!isVaild()))
        {
            return false;
        }
    }
    // 判断addr的协议和socket的协议是否相同
    if (SYLAR_UNLIKELY(addr->getFamily() != m_family))
    {
        SYLAR_LOG_ERROR(g_logger) << "bind sock.family("
                                  << m_family << ") addr.family(" << addr->getFamily()
                                  << ") not equal, addr=" << addr->toString();
        return false;
    }

    // 检测是否可以将地址绑定到socket上
    int rt = ::bind(m_sock, addr->getAddr(), addr->getAddrlen());
    if (rt)
    {
        SYLAR_LOG_ERROR(g_logger) << "bind error errrno=" << errno
                                  << " errstr=" << strerror(errno);
        return false;
    }
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog)
{
    // 判断当前socket是否有效
    if (!isVaild())
    {
        SYLAR_LOG_ERROR(g_logger) << "listen error sock=-1";
        return false;
    }
    // 监听socket是否有客户端连接
    int rt = ::listen(m_sock, backlog);
    if (rt)
    {
        SYLAR_LOG_ERROR(g_logger) << "listen error errno=" << errno
                                  << " errstr=" << strerror(errno);
        return false;   
    }
    return true;
}

Socket::ptr Socket::accept()
{
    Socket::ptr sock(new Socket(m_family, m_type, m_protocol));  
    int newsock = ::accept(m_sock, nullptr, nullptr);
    if (newsock == -1)
    {
        SYLAR_LOG_ERROR(g_logger) << "accept(" << m_sock << ") errno="
                                << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    if (sock->init(newsock))
    {
        return sock;
    }
    return nullptr;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms)
{
    // 获得远端地址
    m_remoteAddress = addr;
    // 判断socket是否有效
    if (!isVaild()) {
        newSock();
        if (SYLAR_UNLIKELY(!isVaild())) {
            return false;
        }
    }
    // 判断addr的协议簇和socket的协议簇是否相同
    if (SYLAR_UNLIKELY(addr->getFamily() != m_family)) {
        SYLAR_LOG_ERROR(g_logger) << "connect sock.family("
                                  << m_family << ") addr.family(" << addr->getFamily()
                                  << ") not equal, addr=" << addr->toString();
        return false;
    }
    //
    if (timeout_ms == (uint64_t)-1)
    {
        int rt = ::connect(m_sock, addr->getAddr(), addr->getAddrlen());
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "sock=" << m_sock << " connect(" << addr->toString()
                                      << ") error errno=" << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    }
    else
    {
        int rt = ::connect_with_timeout(m_sock, addr->getAddr(),addr->getAddrlen(), timeout_ms); 
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "sock=" << m_sock << " connect(" << addr->toString()
                                      << ") error errno=" << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    }
    m_isConnected = true;
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool Socket::reconnect(uint64_t timeout_ms)
{
    if (!m_remoteAddress)
    {
        return false;
    }
    m_localAddress.reset();
    return connect(m_remoteAddress, timeout_ms);
}

bool Socket::close()
{
    // 当前socket已关闭
    if (m_sock == -1 && m_isConnected == false)
    {
        return true;
    }
    m_isConnected = false;
    if (m_sock != -1)
    {
        ::close(m_sock);
        m_sock = -1;
    }
    return false;
}

ssize_t Socket::send(const void* buffer, size_t length, int flags)
{
    if (isConnected())
    {
        return ::send(m_sock, buffer, length, flags);
    }
    return -1;
}

ssize_t Socket::send(const iovec* buffer, size_t length, int flags)
{
    if (isConnected())
    {
        msghdr msg;
        bzero(&msg, sizeof(msg));
        msg.msg_iov = (iovec*)buffer;
        msg.msg_iovlen = length;
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

ssize_t Socket::sendto(const void* buffer, size_t length, const Address::ptr to,int flags)
{
    if (isConnected())
    {
        return ::sendto(m_sock, buffer, length, flags, to->getAddr(), to->getAddrlen());
    }
    return -1;
}

ssize_t Socket::sendto(const iovec* buffer, size_t length, const Address::ptr to,int flags)
{
    if (isConnected())
    {
        msghdr msg;
        bzero(&msg, sizeof(msg));
        msg.msg_iov = (iovec*)buffer;
        msg.msg_iovlen = length;
        msg.msg_name = to->getAddr();
        msg.msg_namelen = to->getAddrlen();
        return ::sendmsg(m_sock, &msg, flags); 
    }
    return -1;
    
}

ssize_t Socket::recv(void* buf, size_t length, int flags)
{
    if (isConnected())
    {
        return ::recv(m_sock, buf, length,flags);
    }
    return -1;
    
}

ssize_t Socket::recv(iovec* buf, size_t length, int flags)
{
    if (isConnected())
    {
        msghdr msg;
        bzero(&msg,sizeof(msg));
        msg.msg_iov = (iovec*)buf;
        msg.msg_iovlen = length;
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

ssize_t Socket::recvfrom(void* buf, size_t length, const Address::ptr from, int flags)
{
    if (isConnected())
    {
        socklen_t len = from->getAddrlen();
        return ::recvfrom(m_sock, buf, length, flags, from->getAddr(), &len);
    }
    return -1;
}

ssize_t Socket::recvfrom(iovec* buffer, size_t length, const Address::ptr from, int flags)
{
    if (isConnected())
    {
        msghdr msg;
        msg.msg_iov = (iovec*)buffer;
        msg.msg_iovlen = length;
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrlen();
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

Address::ptr Socket::getRemoteAddress()
{
    // 如果socket保存了远端地址，则直接返回
    if (m_remoteAddress)
    {
        return m_remoteAddress;
    }
    // 如果需要获得远端地址
    Address::ptr result;
    switch(m_family)
    {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
            break;
    }
    // 获得result的地址长度，用于getpeername
    socklen_t addrlen = result->getAddrlen();
    // 使用getpeername来获得远端地址
    int rt = ::getpeername(m_sock, result->getAddr(), &addrlen);
    if (rt)
    {
        SYLAR_LOG_ERROR(g_logger) << "getpeername error sock=" << m_sock
                                  << " errno=" << errno << " errstr=" << strerror(errno);
        return Address::ptr(new UnknownAddress(m_family));
    }
    // 如果是UNIX，则重新设置地址长度
    if (m_family == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrlen(addrlen);
    }
    // 设置当前socket的远端地址并返回
    m_remoteAddress = result;
    return result;
}

Address::ptr Socket::getLocalAddress()
{
    // 如果本地地址存在，则直接返回
    if (m_localAddress)
    {
        return m_localAddress;
    }
    // 否则如果需要获得本地地址
    Address::ptr result;
    switch(m_family)
    {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
            break;
    }
    // 获取result的地址长度
    socklen_t addrlen = result->getAddrlen();
    // 通过getsockname获得本地地址
    int rt = getsockname(m_sock, result->getAddr(), &addrlen);
    if (rt)
    {
        SYLAR_LOG_ERROR(g_logger) << "getsockname error sock=" << m_sock
                                  << " errno=" << errno << " errstr=" << strerror(errno);
        return Address::ptr(new UnknownAddress(m_family));
    }
     // 如果是UNIX，则重新设置地址长度
     if (m_family == AF_UNIX) 
     {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrlen(addrlen);
    }
    // 设置当前socket的远端地址并返回
    m_localAddress = result;
    return m_localAddress;
}

bool Socket::cancelRead()
{
    return IOManager::GetThis()->cancelEvent(m_sock, IOManager::READ);
}

bool Socket::cancelWrite()
{
    return IOManager::GetThis()->cancelEvent(m_sock,IOManager::WRITE);
}

bool Socket::cancelAccept()
{
    return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::READ);
}

bool Socket::cancelAll()
{
    return IOManager::GetThis()->cancelAll(m_sock);
}
}