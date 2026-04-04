#pragma once

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include "../../include/base/sylar.h"

static sylar::ConfigVar<int>::ptr g_board_size =
    sylar::Config::Lookup("gomoku.borad", "棋盘大小", 15);

static sylar::ConfigVar<std::string>::ptr g_ai_player = 
    sylar::Config::Lookup("gomoku.ai_player", "AI执棋颜色", std::string("white"));

static sylar::ConfigVar<std::string>::ptr g_human_player = 
    sylar::Config::Lookup("gomoku.human_player", "人类执棋颜色", std::string("black"));

class AiGame
{
public:
    using ptr = std::shared_ptr<AiGame>;
    using MutexType = sylar::Mutex;

    /**
     *  @brief 构造函数
     *  @param[in] userId 正在进行这局 AI 五子棋游戏的用户的唯一标识ID。
     */
    AiGame(int userId);

    /**
     *  @brief 判断是否平局 
     */
    bool isDraw() const;

    /**
     *  @brief 玩家下棋 
     */
    bool humanMove(int x, int y);

    /**
     *  @brief 检查玩家是否赢得棋局 
     */
    bool checkWin(int x, int y, const std::string& player);

    /**
     *  @brief ai下棋 
     */
    void aiMove();

    /**
     *  @brief 获取上一步移动的坐标
     */
    std::pair<int, int> getLastMove() const;

    /**
     *  @brief // 获取当前棋盘状态 
     */
    const std::vector<std::vector<std::string>>& getBoard() const;

    /**
     *  @brief 判断游戏是否结束 
     */
    bool isGameOver() const;

    /**
     *  @brief 返回棋局的胜利者
     */
    std::string getWinner() const;
    
private:
    /**
     *  @brief 检查移动是否有效 
     */
    bool isValidMove(int x, int y) const;

    /**
     * @brief 检查坐标是否在棋盘内  
     */
    bool isInBoard(int x, int y) const { return x >= 0 && x < g_board_size->getValue() && y >= 0 && y < g_board_size->getValue(); }

    /**
     *  @brief 获取AI的最佳移动位置
     */
    std::pair<int, int> getBestMove();

    /**
     *  @brief 评估威胁函数 
     */
    int evaluateThreat(int r, int c);

    /**
     *  @brief 判断某个空位是否靠近已有棋子
     */
    bool isNearOccupied(int r, int c);

private:
    bool m_gameover;                                // 判断游戏是否结束
    int m_userId;                                   // 用户ID
    int m_moveCount;                                // 可移动次数
    std::string m_winner{"none"};                   // 胜利者
    std::pair<int, int> m_lastMove{-1, -1};         // 上次移动的位置
    std::vector<std::vector<std::string>> m_board;  // 棋盘
    mutable MutexType m_mutex;                      // 互斥锁
};