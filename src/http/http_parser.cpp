#include "http_parser.h"
#include "../base/log.h"
#include "../base/config.h"


namespace sylar{
namespace http{

/*----------------------------------  全局配置  --------------------------------------*/
static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("http");

static sylar::ConfigVar<uint64_t>::ptr g_http_request_buffer_size = 
    sylar::Config::Lookup("http.request.buffer_size", "http request buffer size", (uint64_t)(4 * 1024));

static sylar::ConfigVar<uint64_t>::ptr g_http_request_max_body_size = 
    sylar::Config::Lookup("http.request.max_body_size", "http request max body size", (uint64_t)(64 * 1024 * 1024));

static sylar::ConfigVar<uint64_t>::ptr g_http_response_buffer_size = 
    sylar::Config::Lookup("http.reponse.buffer_size", "http response buffer size", (uint64_t)(4 * 1024));

static sylar::ConfigVar<uint64_t>::ptr g_http_response_max_body_size = 
    sylar::Config::Lookup("http.reponse.max_body_size", "http response max body size", (uint64_t)(64 * 1024 * 1024));

static uint64_t s_http_request_buffer_size    = 0;
static uint64_t s_http_request_max_body_size  = 0;
static uint64_t s_http_response_buffer_size   = 0;
static uint64_t s_http_response_max_body_size = 0;    


/*----------------------------------  全局函数  --------------------------------------*/
uint64_t HttpRequestParser::GetHttpRequestBufferSize() 
{
    return s_http_request_buffer_size;
}

uint64_t HttpRequestParser::GetHttpRequestMaxBodySize() 
{
    return s_http_request_max_body_size;
}

uint64_t HttpResponseParser::GetHttpResponseBufferSize() 
{
    return s_http_response_buffer_size;
}

uint64_t HttpResponseParser::GetHttpResponseMaxBodySize() 
{
    return s_http_response_max_body_size;
}


/*----------------------------------  静态函数初始化位于main函数执行之前 --------------------------------------*/
struct _HttpSizeIniter
{
    _HttpSizeIniter()
    {
        s_http_request_buffer_size    = g_http_request_buffer_size->getValue();
        s_http_request_max_body_size  = g_http_request_max_body_size->getValue();
        s_http_response_buffer_size   = g_http_response_buffer_size->getValue();
        s_http_response_max_body_size = g_http_response_max_body_size->getValue();  

        g_http_request_buffer_size->addlistener(
            [](const uint64_t &ov, const uint64_t &nv) {
                s_http_request_buffer_size = nv;
            });

        g_http_request_max_body_size->addlistener(
            [](const uint64_t &ov, const uint64_t &nv) {
                s_http_request_max_body_size = nv;
            });

        g_http_response_buffer_size->addlistener([](const uint64_t& oldvalue, const uint64_t& newvalue){
            s_http_response_buffer_size = newvalue;
        });

        g_http_response_max_body_size->addlistener([](const uint64_t& oldvalue, const uint64_t& newvalue){
            s_http_response_max_body_size = newvalue;
        });
    }
};

static _HttpSizeIniter _init;


/*----------------------------------  HttpReqeuse 响应回调函数  --------------------------------------*/

/**
 *  @brief http请求开始解析回调函数 
 */
static int on_request_message_begin_cb(http_parser* p)
{
    // SYLAR_LOG_INFO(g_logger) << "on_request_message_begin_cb";
    return 0;
}
    
/**
 *  @brief http请求头部字段解析结束回调，可获取头部信息字段，如 method/version 等
 *  @note 返回0表示成功，返回1表示该HTTP消息无消息体，返回2表示无消息体并且该连接后续不会再有消息
 */
static int on_request_headers_complete_cb(http_parser* p)
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_request_headers_complete_cb";
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(p->data);
    parser->getHttpRequest()->setVersion(((p->http_major) << 0x4 ) | (p->http_minor));
    parser->getHttpRequest()->setMethod((HttpMethod)(p->method));
    return 0;
}

/**
 *  @brief http请求解析结束回调 
 */
static int on_request_message_complete_cb(http_parser* p)
{
    // SYLAR_LOG_DEBUG(g_logger) <<"on_request_message_complete_cb";
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(p->data);
    parser->setFinished(true);
    http_parser_pause(p, 1);
    return 0;
}

/**
 * @brief http分段头部回调，当前分块开始时触发的回调，告诉你这个分块的长度，准备接收数据
 */
static int on_request_chunk_header_cb(http_parser *p) 
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_request_chunk_header_cb";
    return 0;
}

/**
 * @brief http分段结束回调，当前分块结束时触发的回调，表示这个分块的数据已经接收完毕，可以处理或存储了
 */
static int on_request_chunk_complete_cb(http_parser *p) 
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_request_chunk_complete_cb";
    return 0;
}

/**
 *  @brief http请求url解析完成回调 
 */
static int on_request_url_cb(http_parser* p, const char* buf, size_t len)
{
    // SYLAR_LOG_DEBUG(g_logger) << " on_request_url_cb";
    // 定义返回值
    int rt;
    // 定义url解析结构体
    struct http_parser_url url_parser;
    // 获取http_parser 保存的HttpRequest
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(p->data);
    // 初始化url_parser
    http_parser_url_init(&url_parser);
    // 解析buf，将解析后的结果存放在url_parser
    rt = http_parser_parse_url(buf, len, 0, &url_parser);
    // 根据返回判断是否解析成功
    if (rt != 0)
    {
        SYLAR_LOG_DEBUG(g_logger) << "parse url failure";
        return 1;
    } 
    // 这说明url中包含 path
    if (url_parser.field_set & (1 << UF_PATH))
    {
        parser->getHttpRequest()->setPath(std::string(buf + url_parser.field_data[UF_PATH].off, url_parser.field_data[UF_PATH].len));
    }
    // 这说明url中包含 query
    if (url_parser.field_set & (1 << UF_QUERY))
    {
      
        parser->getHttpRequest()->setQuery(std::string(buf + url_parser.field_data[UF_QUERY].off, url_parser.field_data[UF_QUERY].len));
    }
    // 这说明url中包含 fragment
    if (url_parser.field_set & (1 << UF_FRAGMENT)) 
    {
        parser->getHttpRequest()->setFragment(std::string(buf + url_parser.field_data[UF_FRAGMENT].off, url_parser.field_data[UF_FRAGMENT].len));
    }
    return 0;
}

/**
 *  @brief http请求首部字段名称field解析完成回调 
 */
static int on_request_header_field_cb(http_parser* p, const char* buf, size_t len)
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_request_header_field_cb";
    std::string field(buf, len);
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(p->data);
    parser->setField(field);
    return 0;
}

/**
 *  @brief http请求首部字段值value解析完成回调 
 */
static int on_request_header_value_cb(http_parser* p, const char* buf, size_t len)
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_request_header_value_cb";
    std::string value(buf, len);
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(p->data);
    parser->getHttpRequest()->setHeader(parser->getField(), value);
    return 0;
}

/**
 *   http请求响应状态回调，这个回调没有用，因为http请求不带状态
 */
static int on_request_status_cb(http_parser *p, const char *buf, size_t len) 
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_request_status_cb, should not happen";
    return 0;
}

/**
 *  @brief http消息体回调
 *  @note 当传输编码是块(chunked)，每个chunked数据段都会触发一次当前时间的回调，所以用append的方法将所有数据组合到一起
 */
static int on_request_body_cb(http_parser* p, const char* buf, size_t len)
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_request_body_cb";
    std::string body(buf,len);
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(p->data);
    parser->getHttpRequest()->appendBody(body);
    return 0;
}

static http_parser_settings s_request_settings = {
    .on_message_begin    = on_request_message_begin_cb,
    .on_url              = on_request_url_cb,
    .on_status           = on_request_status_cb,
    .on_header_field     = on_request_header_field_cb,
    .on_header_value     = on_request_header_value_cb,
    .on_headers_complete = on_request_headers_complete_cb,
    .on_body             = on_request_body_cb,
    .on_message_complete = on_request_message_complete_cb,
    .on_chunk_header     = on_request_chunk_header_cb,
    .on_chunk_complete   = on_request_chunk_complete_cb
};


/*----------------------------------  HttpResponse 响应回调函数  --------------------------------------*/

/**
 * @brief http响应开始解析回调函数
 */
static int on_response_message_begin_cb(http_parser* p)
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_response_message_begin_cb";
    return 0;
}

/**
 * @brief http状态行解析结束，可获取状态行字段，如 version、status_codee 等
 * @note 返回0表示成功，返回1表示该HTTP消息无消息体，返回2表示无消息体并且该连接后续不会再有消息
 */
static int on_response_headers_complete_cb(http_parser* p)
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_response_message_field_cb";
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(p->data);
    parser->getData()->setVersion((p->http_major) << 0x4 || (p->http_minor));
    parser->getData()->setStatus((HttpStatus)p->status_code);
    return 0;
}

/**
 * @brief http分段头部回调，可获取分段长度
 */
static int on_response_chunk_header_cb(http_parser* p)
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_response_chunk_header_cb";
    return 0;
}

/**
 * @brief http分段结束回调，表示全部分段已解析完成
 */
static int on_response_chunk_complete_cb(http_parser *p) 
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_response_chunk_complete_cb";
    return 0;
}

/**
 * @brief http响应url解析完成回调，这个回调没有意义，因为响应不会携带url
 */
static int on_response_url_cb(http_parser *p, const char *buf, size_t len) 
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_response_url_cb, should not happen";
    return 0;
}

/**
 * @brief http响应 首部字段名称(field) 解析完成回调
 */
static int on_response_header_field_cb(http_parser* p, const char* buf, size_t len)
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_response_message_field_cb";
    std::string field(buf,len);
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(p->data);
    parser->setField(field);
    return 0;
}

/**
 * @brief http响应 首部字段的值名称(value) 解析完成回调
 */
static int on_response_header_value_cb(http_parser*p ,const char* buf, size_t len)
{
    // SYLAR_LOG_DEBUG(g_logger) << " on_response_message_field_cb";
    std::string value(buf, len);
    HttpResponseParser* parser = static_cast<HttpResponseParser*>(p->data);
    parser->getData()->setHeader(parser->getField(), value);
    return 0;
}

/**
 * @brief http响应状态回调
 */
static int on_response_status_cb(http_parser *p, const char *buf, size_t len) 
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_response_status_cb, status code is: " << p->status_code << ", status msg is: " << std::string(buf, len);
    HttpResponseParser *parser = static_cast<HttpResponseParser *>(p->data);
    parser->getData()->setStatus(HttpStatus(p->status_code));
    return 0;
}

/**
 * @brief http响应消息体回调
 */
static int on_response_body_cb(http_parser *p, const char *buf, size_t len)
 {
    // SYLAR_LOG_DEBUG(g_logger) << "on_response_body_cb, body is:\n" << body;
    std::string body(buf, len);
    HttpResponseParser *parser = static_cast<HttpResponseParser *>(p->data);
    parser->getData()->appendBody(body);
    return 0;
}

/**
 * @brief http响应解析结束回调
 */
static int on_response_message_complete_cb(http_parser *p) 
{
    // SYLAR_LOG_DEBUG(g_logger) << "on_response_message_complete_cb";
    HttpResponseParser *parser = static_cast<HttpResponseParser *>(p->data);
    parser->setFinished(true);
    return 0;
}

static http_parser_settings s_response_settings = 
{
    .on_message_begin    = on_response_message_begin_cb,
    .on_url              = on_response_url_cb,
    .on_status           = on_response_status_cb,
    .on_header_field     = on_response_header_field_cb,
    .on_header_value     = on_response_header_value_cb,
    .on_headers_complete = on_response_headers_complete_cb,
    .on_body             = on_response_body_cb,
    .on_message_complete = on_response_message_complete_cb,
    .on_chunk_header     = on_response_chunk_header_cb,
    .on_chunk_complete   = on_response_chunk_complete_cb
};





/*----------------------------------  HttpRequest Parser  --------------------------------------*/
HttpRequestParser::HttpRequestParser()
{
    http_parser_init(&m_parser, HTTP_REQUEST);
    m_data.reset(new HttpRequest);
    m_parser.data = this;
    m_error = 0;
    m_finished = false;
}

size_t HttpRequestParser::execute(char* data, size_t len)
{
    size_t nparsed = http_parser_execute(&m_parser, &s_request_settings, data, len);
    bool paused_on_message = m_finished && HTTP_PARSER_ERRNO(&m_parser) == HPE_PAUSED;
    if (m_parser.upgrade)
    {
        //处理新协议，暂时不处理
        SYLAR_LOG_DEBUG(g_logger) << "found upgrade, ignore";
        setError(HPE_UNKNOWN);
    }
    else if (m_parser.http_errno != 0 && !paused_on_message)
    {
        SYLAR_LOG_DEBUG(g_logger) << "parse request fail: " << http_errno_name(HTTP_PARSER_ERRNO(&m_parser));
        setError((int8_t)m_parser.http_errno);
    }
    else
    {
        if (nparsed < len)
        {
            memmove(data, data + nparsed, (len - nparsed));
        }
    }
    return nparsed;
}


/*----------------------------------  HttpReponse Parser  --------------------------------------*/

HttpResponseParser::HttpResponseParser() 
{
    http_parser_init(&m_parser, HTTP_RESPONSE);
    m_data.reset(new HttpResponse);
    m_parser.data = this;
    m_error       = 0;
    m_finished    = false;
}

size_t HttpResponseParser::execute(char *data, size_t len) 
{
    size_t nparsed = http_parser_execute(&m_parser, &s_response_settings, data, len);
    if (m_parser.http_errno != 0) 
    {
        SYLAR_LOG_DEBUG(g_logger) << "parse response fail: " << http_errno_name(HTTP_PARSER_ERRNO(&m_parser));
        setError((int8_t)m_parser.http_errno);
    } 
    else 
    {
        if (nparsed < len) 
        {
            memmove(data, data + nparsed, (len - nparsed));
        }
    }
    return nparsed;
}

}
}
