#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include "../../../../include/http/base/http_server.h"
#include "../../../../include/db/mysql.h"
#include "../../../../include/base/util.h"
#include "chatserver_runtime.h"
#include "ai_speech_processer.h"
#include "aihelper.h"
#include "image_recognizer.h"
#include "base64.h"
#include "mqmanager.h"


class ChatLogin;
class ChatRegister;
class ChatLogout;
class Chat;
class ChatEntry;
class ChatSend;
class ChatHistory;
class AiMenu;
class AiUpload;
class AiUploadSend;
class ChatCreateAndSend;
class ChatSessions;
class ChatSpeech;

class ChatServer : public std::enable_shared_from_this<ChatServer> {
public:
    using RWMutexType = sylar::RWMutex;
    using ptr = std::shared_ptr<ChatServer>;

private:
	friend class ChatLogin;
	friend class ChatRegister;
	friend class ChatLogout;
	friend class Chat;
	friend class ChatEntry;
	friend class ChatSend;
	friend class AiMenu;
	friend class AiUpload;
	friend class AiUploadSend;
	friend class ChatHistory;

	friend class ChatCreateAndSend;
	friend class ChatSessions;
	friend class ChatSpeech;

public:
	/**
     *  @brief 构造函数
     */
	ChatServer(bool keepalive = true
                , sylar::IOManager* worker = sylar::IOManager::GetThis()
                , sylar::IOManager* accept_worker = sylar::IOManager::GetThis()
                , bool use_ssl = true
                , sylar::ssl::SslContext::ptr ssl_ctx = nullptr
                , bool useSession = true
                , bool useMiddle = true);

	/**
     *  @brief 绑定IP和端口 
     */
    bool bind(sylar::Address::ptr address) { return m_httpserver->bind(address); }

    
    /**
     *  @brief 启动服务器 
     */
    void start() { m_httpserver->start(); }
    
	/**
     *  @brief 初始路由 
     */
	void initializeRouter();

    bool isDatabaseAvailable() const { return m_databaseAvailable; }
    void requireDatabase() const;

private:
    /**
     *  @brief 初始化相关组件 
     */
	void initialize();

	/**
     *  @brief 初始会话 
     */
	void initializeSession();

    /**
     *  @brief 初始中间件 
     */
	void initializeMiddleware();
	

	void readDataFromMySQL();
    
    /**
     *  @brief 打包HttpResponse，准备发送给客户端
     */
	void packageResp(uint8_t version, sylar::http::HttpStatus status,
                    const std::string& statusMsg, bool close, const std::string& contentType,
                    int cttlen, const std::string& body, sylar::http::HttpResponse::ptr rsp);

private:
	sylar::IOManager* m_worker;                         // 工作线程
    sylar::IOManager* m_accept;                         // 接收请求的连接线程
	sylar::http::HttpServer::ptr m_httpserver;          // http服务器
    sylar::db::MysqlUtil m_mysql;                       // mysql数据库连接池
	std::unordered_map<int, bool> m_onlineUsers;        // 用户是否在线
	RWMutexType	mutexForOnlineUsers_;                   // 在线用户读写锁
	std::unordered_map<int, std::unordered_map<std::string,std::shared_ptr<AIHelper>>> chatInformation;
	RWMutexType	mutexForChatInformation;
	std::unordered_map<int, std::shared_ptr<ImageRecognizer> > ImageRecognizerMap;
	RWMutexType	mutexForImageRecognizerMap;
	std::unordered_map<int,std::vector<std::string> > sessionsIdsMap;
	RWMutexType mutexForSessionsId;
    bool m_databaseAvailable{false};
    std::string m_databaseError;
	
};
