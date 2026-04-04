#include "http_session.h"
#include "http_parser.h"
#include <cstring>

namespace sylar{
namespace http{

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");


HttpSession::HttpSession(SocketStream::ptr stream, bool owner)
    : m_stream(stream)
{}

HttpRequest::ptr HttpSession::recvRequest() 
{
    HttpRequestParser::ptr parser(new HttpRequestParser);
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    std::shared_ptr<char> buffer(new char[buff_size], [](char* ptr) { delete[] ptr; }); 
    char* data = buffer.get();
    int offset = 0;

    if (!m_recvBuffer.empty())
    {
        if (m_recvBuffer.size() > buff_size)
        {
            m_stream->close();
            m_recvBuffer.clear();
            return nullptr;
        }
        offset = (int)m_recvBuffer.size();
        memcpy(data, m_recvBuffer.data(), offset);
        m_recvBuffer.clear();
    }

    while (true)
    {
        if (offset > 0)
        {
            size_t nparse = parser->execute(data, offset);
            if (parser->hasError())
            {
                m_stream->close();
                m_recvBuffer.clear();
                return nullptr;
            }

            offset -= (int)nparse;
            if (offset == (int)buff_size && !parser->isFinished())
            {
                m_stream->close();
                m_recvBuffer.clear();
                return nullptr;
            }

            if (parser->isFinished())
            {
                if (offset > 0)
                {
                    m_recvBuffer.assign(data, offset);
                }
                parser->getHttpRequest()->init();
                return parser->getHttpRequest();
            }
        }

        int len = m_stream->read(data + offset, buff_size - offset);
        if (len <= 0)
        {
            m_stream->close();
            m_recvBuffer.clear();
            return nullptr;
        }

        SYLAR_LOG_DEBUG(g_logger) << "read bytes: " << len;
        offset += len;
    }
}

int HttpSession::sendResponse(HttpResponse::ptr rsp) 
{
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return m_stream->writeFixSize(data.c_str(), data.size());
}

void HttpSession::close()
{
    m_stream->close();
    m_recvBuffer.clear();
}

}
}
