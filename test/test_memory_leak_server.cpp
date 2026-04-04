#include "sylar.h"
#include "log.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
static sylar::http::HttpServer::ptr g_server;

namespace {

constexpr int kPort = 8030;
constexpr const char* kRoute = "/sylar/leak";

struct RequestContext {
    std::string body;
    std::vector<char> cache;
};

bool send_one_request() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        ::close(fd);
        return false;
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return false;
    }

    std::string req = std::string("GET ") + kRoute + " HTTP/1.1\r\n"
        + "Host: 127.0.0.1\r\n"
        + "Connection: close\r\n\r\n";

    ssize_t sent = ::send(fd, req.data(), req.size(), 0);
    if (sent < 0) {
        ::close(fd);
        return false;
    }

    char buffer[1024];
    while (::recv(fd, buffer, sizeof(buffer), 0) > 0) {
    }

    ::close(fd);
    return true;
}

void drive_requests_and_stop() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (int i = 0; i < 3; ++i) {
        int retry = 0;
        while (retry++ < 20 && !send_one_request()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (g_server) {
        g_server->stop();
    }
}

void run_server() {
    sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("127.0.0.1:8030");
    if (!addr) {
        SYLAR_LOG_ERROR(g_logger) << "get address error";
        return;
    }

    g_server.reset(new sylar::http::HttpServer(
        true,
        sylar::IOManager::GetThis(),
        sylar::IOManager::GetThis(),
        false,
        nullptr,
        false,
        false));

    auto dispatch = g_server->getServletDispatch();
    dispatch->addServlet(kRoute, [](sylar::http::HttpRequest::ptr req,
                                    sylar::http::HttpResponse::ptr rsp,
                                    sylar::http::HttpSession::ptr session) {
        (void)session;

        // Intentional leak for Valgrind/ASan demonstration.
        RequestContext* ctx = new RequestContext();
        ctx->body = req->toString();
        ctx->cache.resize(64 * 1024, 'x');

        rsp->setHeader("Content-Type", "text/plain; charset=utf-8");
        rsp->setBody("LEAK\n");
        return 0;
    });

    while (!g_server->bind(addr)) {
        SYLAR_LOG_ERROR(g_logger) << "bind " << *addr << " fail";
        sleep(1);
    }

    std::thread client_thread(drive_requests_and_stop);
    client_thread.detach();

    g_server->start();
}

}  // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    auto root_logger = SYLAR_LOG_ROOT();
    root_logger->setLoggerLevel(sylar::LogLevel::FATAL);
    root_logger->clearAppenders();

    auto system_logger = SYLAR_LOG_NAME("system");
    system_logger->setLoggerLevel(sylar::LogLevel::FATAL);
    system_logger->clearAppenders();

    auto http_logger = SYLAR_LOG_NAME("http");
    http_logger->setLoggerLevel(sylar::LogLevel::FATAL);
    http_logger->clearAppenders();

    sylar::IOManager iom(2);
    iom.schedule(run_server);
    return 0;
}
