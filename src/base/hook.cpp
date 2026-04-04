#include "hook.h"
#include <dlfcn.h>

#include <functional>
#include <cstdarg>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "config.h"
#include "log.h"
#include "fiber.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "macro.h"
#include "singleton.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");


namespace sylar
{
static thread_local bool t_hook_enable = false;

static uint64_t s_connect_timeout = -1;

static sylar::ConfigVar<int>::ptr g_tcp_connect_timeout = 
    sylar::Config::Lookup("tcp.connect.timeout", "tcp connect timeout", 5000);


#define HOOK_FUN(xx) \
    xx(sleep)\
    xx(usleep)\
    xx(nanosleep) \
    xx(socket) \
    xx(connect) \
    xx(accept) \
    xx(read) \
    xx(readv) \
    xx(recv) \
    xx(recvfrom) \
    xx(recvmsg) \
    xx(write) \
    xx(writev) \
    xx(send) \
    xx(sendto) \
    xx(sendmsg) \
    xx(close) \
    xx(fcntl) \
    xx(ioctl) \
    xx(getsockopt) \
    xx(setsockopt)

/**
 *  @brief 用于进行hook初始化 
 *  @details 通过使用dlsym在动态库中寻找宏变量name的函数地址并将其强制转换成(name_fun)赋值给(name_f)
 *  @example sleep_f = (sleep_fun)dlsym(RTLD_NEXT, sleep)
 */
void hook_init()
{
    static bool is_inited = false;
    if (is_inited)
    {
        return;
    }
    // 使用 dlsym 来获取函数指针并赋值
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX);
#undef XX
}

/**
 *  @brief 这段代码的意义任然是在 main 函数之前就执行，这样就可以提前注册好hook的函数 
 */
struct _HookIniter
{
    _HookIniter()
    {
        // 进行 hook 初始化----> 用来hook系统调用
        hook_init();  
        // 通过配置模块来设置超时时间
        s_connect_timeout = g_tcp_connect_timeout->getValue();
        // 设置监听器用来查看配置是否发生变化
        g_tcp_connect_timeout->addlistener([](const int& old_value, const int& new_value){
            SYLAR_LOG_INFO(g_logger) << "tcp connect timeout changed form "
                                     << old_value << "to " << new_value;
            s_connect_timeout = new_value;
        });
    }
};

static _HookIniter s_hook_initer;


bool is_hook_enable()
{
    return t_hook_enable;
}

void set_hook_enable(bool flag)
{
    t_hook_enable = flag;
}

}

/*<-------------------------------------------------------------------->*/

struct timer_info
{
    int cancelled = 0;
};

/**
 *  @brief  把传进来的任意 IO 函数（如 read、write、recv）及其参数原封不动地“转发”给真正的系统调用，但在需要 hook 的时候可以加以拦截、处理。
 *  @param[in]  fd  文件句柄
 *  @param[in]  fun 函数指针
 *  @param[in]  hook_fun_name   函数名称
 *  @param[in]  event   事件类型(READ | WRITE)
 *  @param[in]  timeout_so  超时时间
 *  @param[in]  args 可变参数
 *  @details OriginFun 是原始函数指针类型:  read--> ssize_t (*)(int fd, void* buf, size_t bufsize)
 *           args      是函数调用时的可变参数列表，比如 void* buf, size_t size
 * 
 */
template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name, uint32_t event, int timeout_so, Args&&... args)
{
    // 这是判断是否启用hook
    if (!sylar::t_hook_enable)
    {
        // 如果没有启用 hook，直接调用原始系统调用函数
        return fun(fd, std::forward<Args>(args)...);
    }

    // 这是获得 fd 的 上下文（是否 socket、是否设置非阻塞、超时设置等信息）
    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    if (!ctx)
    {
        // 如果失败获取 fdCtx，直接调用原始系统调用函数
        return fun(fd, std::forward<Args>(args)...);
    }
    
    // 判断该fd是否已经关闭
    if (ctx->isClosed())
    {
        // 如果fd已关闭，设置错误码errno
        errno = EBADF;
        return -1;
    }

    // 如果不是 socket 或是用户自己设置了非阻塞模式，不 hook
    if (!ctx->isSocket() || ctx->getUserNonblock())
    {
        // 直接返回系统调用
        return fun(fd, std::forward<Args>(args)...);
    }
    
    // 获取当前 fd 的超时时间
    uint64_t to = ctx->getTimeout(timeout_so);
    // 构建一个 timer_info（协程阻塞等待的超时信息）用于后续协程恢复判断
    std::shared_ptr<timer_info> tinfo(new timer_info);
retry:
    // 尝试执行系统调用函数
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    // n == -1 表示执行失败，并且 errno == EINTR 表示程序被中断，重新执行
    while (n == -1 && errno == EINTR)
    {
        n = fun(fd, std::forward<Args>(args)...);
    }
    // errno == EAGAIN 这表示当前不可以进行读写，处于阻塞状态，所以需要当前协程让出执行权
    if (n == -1 && errno == EAGAIN)
    {
        // 获得当前IO协程调度器
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        // 设置一个定时器
        sylar::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);
        // 判断是否需要设置超时计时器
        if (to != (uint64_t)-1)
        {
            // 给IO协程调度器添加一个条件定时器
            timer = iom->addTimerCondition(to, false,[winfo, fd, iom, event](){
                auto t = winfo.lock();
                if (!t || t->cancelled)
                {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, static_cast<sylar::IOManager::Event>(event));
            }, winfo);
        }
        // 将 fd 对应的事件 event 添加到 epoll 内核事件表
        int rt = iom->addEvent(fd, static_cast<sylar::IOManager::Event>(event));
        // 这表示往epoll添加事件失败
        if (SYLAR_UNLIKELY(rt))
        {
            SYLAR_LOG_ERROR(g_logger) << hook_fun_name << " addEvent("
                                      << fd << ", " << event << ")";
            if(timer) 
            {
                timer->cancel();
            }
            return -1;
        }
        // 这表示往epoll添加事件成功
        else
        {
            // 当前协程让出执行权
            sylar::Fiber::GetThis()->yield();
            if (timer)
            {
                timer->cancel();
            }
            if (tinfo->cancelled)
            {
                errno = tinfo->cancelled;
                return -1;
            }
            goto retry;            
        } 
    }
    
    
    return n;
}






/*<-------------------------------------------------------------------->*/
extern "C"
{

#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX


/*-----------------  sleep  ----------------------------*/

unsigned int sleep(unsigned int seconds)
{
    // 判断当前线程是否被hook
    if (!sylar::t_hook_enable)
    {
        return sleep_f(seconds);
    }
    // 这说明当前线程被hook
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "hook start";
    sylar::Fiber::ptr fiber  = sylar::Fiber::GetThis();         // 获得当前线程正在执行的协程
    sylar::IOManager* iom = sylar::IOManager::GetThis();        // 获得当前线程所属的IO协程调度器
    // 
    iom->addTimer(seconds * 1000, false, [iom, fiber](){
        SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "Timer triggered, scheduling fiber.";
        iom->schedule(fiber);
    });
    sylar::Fiber::GetThis()->yield();                    // 因为要睡眠，所以要让出执行权
    return 0;
}

int usleep(useconds_t usec)
{
    if(!sylar::t_hook_enable)
    {
        return usleep_f(usec);
    }
    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(usec/1000, false, [iom, fiber](){
        iom->schedule(fiber);
    });
    sylar::Fiber::GetThis()->yield();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (!sylar::t_hook_enable)
    {
       return nanosleep_f(req, rem);
    }

    int timeout_ms = req->tv_sec * 1000 +  req->tv_nsec/1000/1000;
    sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    iom->addTimer(timeout_ms, false, [iom, fiber]() {
        iom->schedule(fiber, -1);
    });
    sylar::Fiber::GetThis()->yield();
    return 0;
}


/*------------------  socket  ----------------------------*/
int socket(int domain, int type, int protocol)
{
    if (!sylar::t_hook_enable)
    {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if (fd == -1)
    {
        return fd;
    }
    sylar::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms)
{
    // 判断是否启用 hook
    if (!sylar::t_hook_enable)
    {
        return connect_f(fd, addr, addrlen);
    }
    // 获取 fd 的 fdCtx并检查合法性
    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    if (!ctx || ctx->isClosed())
    {
        errno = EBADF;
        return -1;
    }
    if (!ctx->isSocket())
    {
        return connect_f(fd, addr, addrlen);
    }
    if (ctx->getUserNonblock())
    {
        return connect_f(fd, addr, addrlen);
    }
    
    // 尝试连接fd到给定的地址
    int n = connect_f(fd, addr, addrlen);
    // 这表示直接连接成功
    if (n == 0)     
    {
        return 0;
    }
    // 这表示连接失败
    else if (n != -1 || errno !=  EINPROGRESS)
    {
        return n;
    }
    // 最后代表着连接正在阻塞中
    sylar::IOManager* iom = sylar::IOManager::GetThis();    // 获得当前的协程调度器
    sylar::Timer::ptr timer;                                // 设置一个定时器
    std::shared_ptr<timer_info> tinfo(new timer_info);      // 创建协程使用的 timer_info，用于表示连接是否被取消；
    std::weak_ptr<timer_info> winfo(tinfo);                 
    
    // 表示设置了连接超时时间
    if (timeout_ms != static_cast<uint64_t>(-1))
    {
        // 给协程调度器设置一个条件定时器，在 timeout_ms 启动该定时器
        timer = iom->addTimerCondition(timeout_ms, false, [winfo, fd, iom]() {
            auto t = winfo.lock();  
            if(!t || t->cancelled) {    
                return;
            }
            t->cancelled = ETIMEDOUT;   // 判断当前的状态为事件超时
            iom->cancelEvent(fd, sylar::IOManager::WRITE);  // 由于该协程被唤醒, 在内核事件表 epoll 上删除该 fd 感兴趣的写事件
    }, winfo);
    }
    // 给 epoll 注册该 fd，监听该 fd 上的事件
    int rt = iom->addEvent(fd, sylar::IOManager::WRITE);
    // 这表示注册成功
    if (rt == 0)
    {
        // 让出当前协程的执行权
        sylar::Fiber::GetThis()->yield();
        // 唤醒后检查是否被定时器取消
        if (timer)
        {
            timer->cancel();
        }
        if (tinfo->cancelled)
        {
            errno = tinfo->cancelled;
            return -1;
        }
    }
    // 这表示注册失败
    else
    {
        // 添加事件失败，也取消定时器
        if (timer)
        {
            timer->cancel();
        }
        SYLAR_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    }
    // 无论是被唤醒还是超时后，都用 getsockopt() 获取 SO_ERROR 判断最终连接结果；SO_ERROR == 0 表示连接成功。
    int error = 0;
    socklen_t len = sizeof(int);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1)
    {
        return -1;
    }
    // error为 0 表示连接成功
    if (!error)
    {
        return 0;
    }
    else
    {
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    return connect_with_timeout(sockfd, addr, addrlen, sylar::s_connect_timeout);
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
    int fd = do_io(sockfd, accept_f, "accept", sylar::IOManager::READ,SO_RCVTIMEO, addr,addrlen);
    if (fd >= 0)
    {
        sylar::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count) 
{
    return do_io(fd, read_f, "read", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) 
{
    return do_io(fd, readv_f, "readv", sylar::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) 
{
    return do_io(sockfd, recv_f, "recv", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) 
{
    return do_io(sockfd, recvfrom_f, "recvfrom", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) 
{
    return do_io(sockfd, recvmsg_f, "recvmsg", sylar::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) 
{
    return do_io(fd, write_f, "write", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) 
{
    return do_io(fd, writev_f, "writev", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) 
{
    return do_io(s, send_f, "send", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) 
{
    return do_io(s, sendto_f, "sendto", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) 
{
    return do_io(s, sendmsg_f, "sendmsg", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd)
{
    if (!sylar::t_hook_enable)
    {
        return close_f(fd);
    }

    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
    if (ctx)
    {
        auto iom = sylar::IOManager::GetThis();
        if (iom)
        {
            iom->cancelAll(fd);
        }
        sylar::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);    
}

int fcntl(int fd, int cmd, ... /* arg */ ) 
{
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
                    return fcntl_f(fd, cmd, arg);
                }
                ctx->setUserNonblock(arg & O_NONBLOCK);
                if(ctx->getSysNonblock()) {
                    arg |= O_NONBLOCK;
                } else {
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClosed() || !ctx->isSocket()) 
                {
                    return arg;
                }
                if(ctx->getUserNonblock()) 
                {
                    return arg | O_NONBLOCK;
                } 
                else 
                {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg); 
            }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int d, unsigned long int request, ...)
 {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if(FIONBIO == request) 
    {
        bool user_nonblock = !!*(int*)arg;
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(d);
        if(!ctx || ctx->isClosed() || !ctx->isSocket()) 
        {
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) 
{
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
 {
    if(!sylar::t_hook_enable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET)
    {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) 
        {
            sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(sockfd);
            if(ctx) 
            {
                const timeval* v = (const timeval*)optval;
                ctx->setTimeOut(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}