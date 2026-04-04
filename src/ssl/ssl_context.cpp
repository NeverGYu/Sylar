#include "../ssl/ssl_context.h"
#include "../base/log.h"
#include <openssl/err.h>

namespace sylar{
namespace ssl{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

SslContext::SslContext(const SslConfig& config)
    : m_config(config)
    , m_ctx(nullptr)
{}

SslContext::~SslContext()
{
    if (m_ctx)
    {
        SSL_CTX_free(m_ctx);
    }
}

bool SslContext::initilaize()
{
    // 初始化OpenSSL
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
    // 创建 SSL 上下文
    const SSL_METHOD* method = TLS_server_method();
    m_ctx = SSL_CTX_new(method);
    if (!m_ctx)
    {
        handleSslError("Failed to create SSL context");
        return false;
    }
    // 设置Ssl选项
    long option = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |SSL_OP_CIPHER_SERVER_PREFERENCE;
    SSL_CTX_set_options(m_ctx, option);
    // 加载证书和密钥
    if (!loadCertificates())
    {
        return false;
    }
    // 设置协议版本
    if (!setupProtocol())
    {
        return false;
    }
    // 设置会话缓存
    setupSessionCache();
    // 表示成功输出
    SYLAR_LOG_INFO(g_logger) << "SSL context initialized successfully";
    return true;
}

bool SslContext::loadCertificates()
{
    // 加载公钥证书
    if (SSL_CTX_use_certificate_file(m_ctx, m_config.getCertificateFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load server certificate");
        return false;
    }
    // 加载私钥
    if (SSL_CTX_use_PrivateKey_file(m_ctx, m_config.getPrivateKeyFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load private key");
        return false;
    }
    // 验证公钥证书和私钥匹配
    if (!SSL_CTX_check_private_key(m_ctx))
    {
        handleSslError("Private key does not match the certificate");
        return false;
    }
    // 用于构建完整的“证书链”（中级CA → 根CA）以供客户端验证。
    if (!m_config.getCertificateChainFile().empty())
    {
        if (SSL_CTX_use_certificate_chain_file(m_ctx,m_config.getCertificateChainFile().c_str()) <= 0)
        {
            handleSslError("Failed to load certificate chain");
            return false;
        }
    }
    // 返回成功
    return true;
}

bool SslContext::setupProtocol()
{
    // 设置 SSL/TLS 协议版本
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    switch (m_config.getVersion())
    {
        case SslVersion::TLS_1_0:
            break;
        case SslVersion::TLS_1_1:
            options |= SSL_OP_NO_TLSv1;
            break;
        case SslVersion::TLS_1_2:
            options |= SSL_OP_NO_TLSv1_1;
            break;
        case SslVersion::TLS_1_3:
            options |= SSL_OP_NO_TLSv1_2;
            break;
    }
    SSL_CTX_set_options(m_ctx, options);
    // 设置加密套件
    if (!m_config.getCipherList().empty())
    {
        if (SSL_CTX_set_cipher_list(m_ctx,m_config.getCipherList().c_str()) <= 0)
        {
            handleSslError("Failed to set cipher list");
            return false;
        }
    }
    return true;
}

void SslContext::setupSessionCache()
{
    SSL_CTX_set_session_cache_mode(m_ctx, SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_cache_size(m_ctx, m_config.getSessionCacheSize());
    SSL_CTX_set_timeout(m_ctx, m_config.getSessionTimeout());
}

void SslContext::handleSslError(const char* msg)
{
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    SYLAR_LOG_ERROR(g_logger) << msg << ": " << buf;
}


}
}