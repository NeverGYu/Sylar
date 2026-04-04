#include "../include/base/gomoku_server.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int main()
{
    sylar::IOManager iom(2);
    iom.schedule([&iom](){
        // 设置 SSL 的配置
        sylar::ssl::SslConfig config;
        config.setCertificateFile("/home/gch/sylar/bin/conf/server.crt");
        config.setPrivateKeyFile("/home/gch/sylar/bin/conf/server.key");
        config.setVersion(sylar::ssl::SslVersion::TLS_1_2);
        // 创建 SSL 上下文
        sylar::ssl::SslContext::ptr ssl_ctx = std::make_shared<sylar::ssl::SslContext>(config);
        // 初始化 SSL 上下文
        if (!ssl_ctx->initilaize()) {
            SYLAR_LOG_ERROR(g_logger) << "Failed to initialize SSL context" << std::endl;
            return;
        }
        GomokuServer::ptr server(new GomokuServer(true, &iom, &iom, true, ssl_ctx));
        server->initializeRouter();
        sylar::Address::ptr address = sylar::Address::LookupAnyIPAddress("0.0.0.0:8443");
        if (!address)
        {
            SYLAR_LOG_INFO(g_logger) << "access addr fails";
        }

        while (!server->bind(address))
        {
            sleep(2);
        }

        server->start();
    });
    return 0;
}