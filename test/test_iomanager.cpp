#include "sylar.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h> 

sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

void test_fiber()
{
    SYLAR_LOG_INFO(g_logger) << "test_fiber";
}

int sockfd;
void watch_io_read();

// 写事件回调，只执行一次，用于判断非阻塞套接字connect成功
void do_io_write() {
    SYLAR_LOG_INFO(g_logger) << "write callback";
    int so_err;
    socklen_t len = size_t(so_err);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_err, &len);
    if(so_err)
    {
        SYLAR_LOG_INFO(g_logger) << "connect fail";
        return;
    } 
    SYLAR_LOG_INFO(g_logger) << "connect success";
}

// 读事件回调，每次读取之后如果套接字未关闭，需要重新添加
void do_io_read() {
    SYLAR_LOG_INFO(g_logger) << "read callback";
    char buf[1024] = {0};
    int readlen = 0;
    readlen = read(sockfd, buf, sizeof(buf));
    if(readlen > 0) 
    {
        buf[readlen] = '\0';
        SYLAR_LOG_INFO(g_logger) << "read " << readlen << " bytes, read: " << buf;
    } 
    else if(readlen == 0) 
    {
        SYLAR_LOG_INFO(g_logger) << "peer closed";
        close(sockfd);
        return;
    } 
    else 
    {
        SYLAR_LOG_ERROR(g_logger) << "err, errno=" << errno << ", errstr=" << strerror(errno);
        close(sockfd);
        return;
    }
    // read之后重新添加读事件回调，这里不能直接调用addEvent，因为在当前位置fd的读事件上下文还是有效的，直接调用addEvent相当于重复添加相同事件
    sylar::IOManager::GetThis()->schedule(watch_io_read);
}


void watch_io_read() {
    SYLAR_LOG_INFO(g_logger) << "watch_io_read";
    sylar::IOManager::GetThis()->addEvent(sockfd, sylar::IOManager::READ, do_io_read);
}


void test1()
{
    sylar::IOManager iom;
    iom.schedule(&test_fiber);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "142.250.191.206", &addr.sin_addr.s_addr);
    int rt = connect(sockfd, (const sockaddr*)&addr, sizeof(addr));
    if (rt != 0)
    {
        if (errno == EINPROGRESS)
        {
            SYLAR_LOG_INFO(g_logger) << "EINPROGRESS";
            // 注册写事件回调，只用于判断connect是否成功
            // 非阻塞的TCP套接字connect一般无法立即建立连接，要通过套接字可写来判断connect是否已经成功
            sylar::IOManager::GetThis()->addEvent(sockfd, sylar::IOManager::WRITE, do_io_write);
            // 注册读事件回调，注意事件是一次性的
            sylar::IOManager::GetThis()->addEvent(sockfd, sylar::IOManager::READ, do_io_read);
        }
        else 
        {
            SYLAR_LOG_ERROR(g_logger) << "connect error, errno:" << errno << ", errstr:" << strerror(errno);
        }
    }

}

int main(int argc, char** argv)
{
    YAML::Node root = YAML::LoadFile("/home/gch/高性能服务器框架/conf/log.yaml");
    sylar::Config::LoadFromYaml(root);
    test1();
    return 0;
}