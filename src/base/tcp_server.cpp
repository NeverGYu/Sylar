#include "tcp_server.h"
#include "config.h"

namespace sylar
{

/*-------------------------------------  日志  --------------------------------------*/
static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");


/*-------------------------------------  配置  --------------------------------------*/
static sylar::ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout =
        sylar::Config::Lookup("tcp_server.read_timeout", "tcp server read timeout", (uint64_t)(60 * 1000 * 2));
    
/*-------------------------------------  函数  --------------------------------------*/
TcpServer::TcpServer(IOManager* ioworker, IOManager* accept)
    : m_ioworker(ioworker)
    , m_acceptworker(accept)
    , m_recvtimeout(g_tcp_server_read_timeout->getValue())
    , m_name("syalr/1.0.0")
    , m_type("tcp")
    , m_isStop(true)
{}

TcpServer::~TcpServer()
{
    for (auto& sock : m_socks)
    {
        sock->close();
    }
    m_socks.clear();
}

bool TcpServer::bind(Address::ptr address)
{
    std::vector<Address::ptr> addrs;
    std::vector<Address::ptr> fails;
    addrs.push_back(address);
    return bind(addrs, fails);
}

bool TcpServer::bind(std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails)
{
    for(auto& addr : addrs)
    {
        Socket::ptr sock = Socket::CreateTCP(addr);
        if (!sock->bind(addr))
        {
            SYLAR_LOG_ERROR(g_logger) << "bind fail errno="
                                      << errno << " errstr=" << strerror(errno)
                                      << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        if (!sock->listen())
        {
            SYLAR_LOG_ERROR(g_logger) << "listen fail errno="
                                      << errno << " errstr=" << strerror(errno)
                                      << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        m_socks.push_back(sock);
    }

    if(!fails.empty()) 
    {
        m_socks.clear();
        return false;
    }

    for(auto& i : m_socks) 
    {
        SYLAR_LOG_INFO(g_logger) << "type=" << m_type
                                 << " name=" << m_name
                                 << " server bind success: " << *i;
    }
    return true;

}

bool TcpServer::start()
{
    if (!m_isStop)
    {
        return true;
    }
    
    m_isStop = false;
    for(auto& sock : m_socks)
    {
        m_acceptworker->schedule(std::bind(&TcpServer::startAccept, shared_from_this(), sock));
    }
    return true;
}

void TcpServer::startAccept(Socket::ptr sock)
{
    while (!m_isStop)
    {
        Socket::ptr client = sock->accept();
        if (client)
        {
            m_ioworker->schedule(std::bind(&TcpServer::handleClient, shared_from_this(), client));
        }
        else
        {
            SYLAR_LOG_ERROR(g_logger) << "accept errno=" << errno
                                      << " errstr=" << strerror(errno);
        }
    } 
}

void TcpServer::handleClient(Socket::ptr client)
{
    SYLAR_LOG_INFO(g_logger) << "handleClient: " << *client;
}

void TcpServer::stop()
{
    m_isStop = true;
    auto it = shared_from_this();
    m_acceptworker->schedule([this, it](){
        for(auto& sock : m_socks)
        {
            sock->cancelAll();
            sock->close();
        }
        m_socks.clear();
    });
}

std::string TcpServer::toString(const std::string& prefix)
{
    std::stringstream ss;
    ss << prefix << "[type=" << m_type
       << " name=" << m_name
       << " io_worker=" << (m_ioworker ? m_ioworker->getName() : "")
       << " accept=" << (m_acceptworker ? m_acceptworker->getName() : "")
       << " recv_timeout=" << m_recvtimeout << "]" << std::endl;
    std::string pfx = prefix.empty() ? "    " : prefix;
    for (auto &i : m_socks)
    {
        ss << pfx << pfx << *i << std::endl;
    }
    return ss.str();
}


}