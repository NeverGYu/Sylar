#include "../include/base/chatServer.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int main()
{
    sylar::IOManager iom(2);
    iom.schedule([&iom](){
        const bool use_ssl = chatserver::envBoolOrDefault("CHAT_SERVER_USE_SSL", true);
        sylar::ssl::SslContext::ptr ssl_ctx = nullptr;

        if (use_ssl) {
            const std::string certPath = chatserver::sslCertPath();
            const std::string keyPath = chatserver::sslKeyPath();

            if (!chatserver::fileExists(certPath) || !chatserver::fileExists(keyPath)) {
                SYLAR_LOG_ERROR(g_logger) << "SSL files not found: cert=" << certPath
                                          << ", key=" << keyPath;
                return;
            }

            sylar::ssl::SslConfig config;
            config.setCertificateFile(certPath);
            config.setPrivateKeyFile(keyPath);
            config.setVersion(sylar::ssl::SslVersion::TLS_1_2);
            ssl_ctx = std::make_shared<sylar::ssl::SslContext>(config);

            if (!ssl_ctx->initilaize()) {
                SYLAR_LOG_ERROR(g_logger) << "Failed to initialize SSL context" << std::endl;
                return;
            }
        }

        ChatServer::ptr server(new ChatServer(true, &iom, &iom, use_ssl, ssl_ctx, true, true));
        server->initializeRouter();
        sylar::Address::ptr address = sylar::Address::LookupAnyIPAddress(chatserver::bindAddress(use_ssl));
        if (!address)
        {
            SYLAR_LOG_INFO(g_logger) << "access addr fails";
            return;
        }

        while (!server->bind(address))
        {
            sleep(2);
        }

        SYLAR_LOG_INFO(g_logger) << "chat_server listening on " << address->toString()
                                 << ", ssl=" << (use_ssl ? "on" : "off");
        server->start();
    });
    return 0;
}
