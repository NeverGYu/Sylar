#include "../../include/servlet/aigame_move.h"

AiGameMove::AiGameMove(GomokuServer::ptr server)
    : Servlet("aigame move")
    , m_server(server)
{}

int32_t AiGameMove::handle(sylar::http::HttpRequest::ptr req, sylar::http::HttpResponse::ptr rsp, sylar::http::HttpSession::ptr session)
{
    try
    {
        auto sessionptr = m_server->m_httpserver->getSessionManager()->getSession(req, rsp);
        if (sessionptr->getValue("isLoggedIn") != "true")
        {
            // 用户未登录，返回未授权错误
            nlohmann::json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);

            m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::UNAUTHORIZED,
                                 "Unauthorized", true, "application/json", errorBody.size(),
                                 errorBody, rsp);
            return -1;
        }

        int userId = std::stoi(sessionptr->getValue("userId"));
        // 解析请求体
        nlohmann::json request = nlohmann::json::parse(req->getBody());
        int x = request["x"];
        int y = request["y"];

        // 获取或创建游戏实例
        if (m_server->m_aiGames.find(userId) == m_server->m_aiGames.end())
        {
            RWMutexType::WriteLock lock(m_server->m_mutex_ai_game);
            m_server->m_aiGames[userId] = std::make_shared<AiGame>(userId);
        }
        auto &game = m_server->m_aiGames[userId];

        // 处理人类玩家移动
        if (!game->humanMove(x, y))
        {
            nlohmann::json response = {
                {"status", "error"},
                {"message", "Invalid move"}};
            std::string responseBody = response.dump();

            rsp->setStatus(sylar::http::HttpStatus::BAD_REQUEST);
            rsp->setClose(false);
            rsp->setHeader("content-type", "application/json");
            rsp->setBody(responseBody);
            return -1;
        }

        // 检查人类玩家是否获胜
        if (game->isGameOver())
        {
            nlohmann::json response = {
                {"status", "ok"},
                {"board", game->getBoard()},
                {"winner", "human"},
                {"next_turn", "none"}};
            std::string responseBody = response.dump();

            rsp->setStatus(sylar::http::HttpStatus::OK);
            rsp->setClose(false);
            rsp->setHeader("content-type", "application/json");
            rsp->setBody(responseBody);

            {
                RWMutexType::WriteLock lock(m_server->m_mutex_ai_game);
                m_server->m_aiGames.erase(userId); // 这里删掉以后，每次restart都需要重新创建就行
            }
            return 0;
        }

        // 检查是否平局（在AI移动之前）
        if (game->isDraw())
        {
            nlohmann::json response = {
                {"status", "ok"},
                {"board", game->getBoard()},
                {"winner", "draw"},
                {"next_turn", "none"}};
            std::string responseBody = response.dump();

            rsp->setStatus(sylar::http::HttpStatus::OK);
            rsp->setClose(false);
            rsp->setHeader("content-type", "application/json");
            rsp->setBody(responseBody);

            {
                RWMutexType::WriteLock lock(m_server->m_mutex_ai_game);
                m_server->m_aiGames.erase(userId); // 这里删掉以后，每次restart都需要重新创建就行
            }
            return 0;
        }

        // AI移动
        game->aiMove();

        // 检查AI是否获胜
        if (game->isGameOver())
        {
            nlohmann::json response = {
                {"status", "ok"},
                {"board", game->getBoard()},
                {"winner", "ai"},
                {"next_turn", "none"},
                {"last_move", {{"x", game->getLastMove().first}, {"y", game->getLastMove().second}}}};
            std::string responseBody = response.dump();

            rsp->setStatus(sylar::http::HttpStatus::OK);
            rsp->setClose(false);
            rsp->setHeader("content-type", "application/json");
            rsp->setBody(responseBody);

            {
                RWMutexType::WriteLock lock(m_server->m_mutex_ai_game);
                m_server->m_aiGames.erase(userId); // 这里删掉以后，每次restart都需要重新创建就行
            }
            return 0;
        }

        // 再次检查是否平局（在AI移动之后）
        if (game->isDraw())
        {
            nlohmann::json response = {
                {"status", "ok"},
                {"board", game->getBoard()},
                {"winner", "draw"},
                {"next_turn", "none"},
                {"last_move", {{"x", game->getLastMove().first}, {"y", game->getLastMove().second}}}};
            std::string responseBody = response.dump();

            rsp->setStatus(sylar::http::HttpStatus::OK);
            rsp->setClose(false);
            rsp->setHeader("content-type", "application/json");
            rsp->setBody(responseBody);

            {
                RWMutexType::WriteLock lock(m_server->m_mutex_ai_game);
                m_server->m_aiGames.erase(userId); // 这里删掉以后，每次restart都需要重新创建就行
            }
            return 0;
        }

        // 游戏继续
        nlohmann::json response = {
            {"status", "ok"},
            {"board", game->getBoard()},
            {"winner", "none"},
            {"next_turn", "human"},
            {"last_move", {{"x", game->getLastMove().first}, {"y", game->getLastMove().second}}}};

        std::string responseBody = response.dump();

        rsp->setStatus(sylar::http::HttpStatus::OK);
        rsp->setClose(false);
        rsp->setHeader("content-type", "application/json");
        rsp->setBody(responseBody);
    }
    catch (const std::exception &e)
    { 
        nlohmann::json response = {
            {"status", "error"},
            {"message", e.what()}};
        std::string responseBody = response.dump();
        m_server->packageResp(req->getVersion(), sylar::http::HttpStatus::INTERNAL_SERVER_ERROR
                        , "Internal Server Error", false, "application/json", responseBody.size(), responseBody, rsp);
    }

    return 0;
}