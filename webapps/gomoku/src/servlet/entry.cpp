#include "../../include/servlet/entry.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

Entry::Entry(GomokuServer::ptr server)
    : Servlet("entry")
    , m_server(server)
{}

int32_t Entry::handle(const sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session)
{
     // 因为是get请求，请求的url也拿到了，我们就可以直接返回响应了
     std::string reqFile;
     reqFile.append("../../webapps/gomoku/resource/entry.html");
     sylar::FileUtil fileOperater(reqFile);
     if (!fileOperater.isValid())
     {
        SYLAR_LOG_WARN(g_logger) << reqFile << " not exist";
        fileOperater.resetDefaultFile(); // 404 NOT FOUND
     }
 
     std::vector<char> buffer(fileOperater.size());
     fileOperater.readFile(buffer); // 读出文件数据
     std::string bufStr = std::string(buffer.data(), buffer.size());
     
     rsp->setStatus(sylar::http::HttpStatus::OK);
     rsp->setClose(false);
     rsp->setHeader("content-type", "text/html");
     rsp->setBody(bufStr);

     return 0;
}