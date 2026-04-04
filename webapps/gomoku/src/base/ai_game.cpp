#include "../base/ai_game.h"

AiGame::AiGame(int userId)
    : m_gameover(false)
    , m_userId(userId)
    , m_moveCount(0)
    , m_lastMove(-1,-1)
    , m_board(g_board_size->getValue(), std::vector<std::string>(g_board_size->getValue(), "empty"))
{
    srand(0);
}

bool AiGame::isDraw() const
{
    MutexType::Lock lock(m_mutex);
    return m_moveCount >= g_board_size->getValue() * g_board_size->getValue();    
}

bool AiGame::humanMove(int x, int y)
{
    if (!isValidMove(x, y))
    {   
        return false;
    }

    m_board[x][y] = g_human_player->getValue();
    m_moveCount++;
    m_lastMove = {x, y};

    if (checkWin(x, y, g_human_player->getValue())) 
    {
        m_gameover = true;
        m_winner = "human";
    }

    return true;
}

bool AiGame::checkWin(int x, int y, const std::string &player)
{
     // 检查方向数组：水平、垂直、对角线、反对角线
     const int dx[] = {1, 0, 1, 1};
     const int dy[] = {0, 1, 1, -1};
     
     for (int dir = 0; dir < 4; dir++) 
     {
         int count = 1;  // 当前位置已经有一个棋子
         
         // 正向检查
         for (int i = 1; i < 5; i++) 
         {
             int newX = x + dx[dir] * i;
             int newY = y + dy[dir] * i;
             if (!isInBoard(newX, newY) || m_board[newX][newY] != player) 
             {
                break;
             }
             count++;
         }
         
         // 反向检查
         for (int i = 1; i < 5; i++) 
         {
             int newX = x - dx[dir] * i;
             int newY = y - dy[dir] * i;
             if (!isInBoard(newX, newY) || m_board[newX][newY] != player)
             { 
                break;
            }
             count++;
         }
         
         if (count >= 5) return true;
     }
     return false;
}

void AiGame::aiMove()
{
    if (m_gameover || isDraw())
    {
        return;
    }
    // 添加500毫秒延时
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
    int x, y;
    // 获取AI的最佳移动位置
    std::tie(x, y) = getBestMove();
    m_board[x][y] = g_ai_player->getValue();
    m_moveCount++;
    m_lastMove = {x, y};
    
    if (checkWin(x, y, g_ai_player->getValue())) 
    {
        m_gameover = true;
        m_winner = "ai";
    }
}

std::pair<int, int> AiGame::getLastMove() const
{
    MutexType::Lock lock(m_mutex);
    return m_lastMove;
}

const std::vector<std::vector<std::string>>& AiGame::getBoard() const 
{
    MutexType::Lock lock(m_mutex);
    return m_board;
}

bool AiGame::isGameOver() const
{
    MutexType::Lock lock(m_mutex);
    return m_gameover;
}

std::string AiGame::getWinner() const
{
    MutexType::Lock lock(m_mutex);
    return m_winner;
}

bool AiGame::isValidMove(int x, int y) const
{
    if (x < 0 || x >= g_board_size->getValue() || y < 0 || y >= g_board_size->getValue())
    {
        return false;
    } 
    if (m_board[x][y] != "empty") 
    {
        return false;
    }
    if (m_gameover|| isDraw()) 
    {
        return false;
    }
    return true;
}

int AiGame::evaluateThreat(int r, int c)
{
    // 威胁分数
    int threat = 0;
    // 检查四个方向上的玩家连子数
    const int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (auto& dir : directions) 
    {
        int count = 1;
        for (int i = 1; i <= 2; i++) 
        { // 探查2步
            int nr = r + i * dir[0], nc = c + i * dir[1];
            if (nr >= 0 && nr < g_board_size->getValue() && nc >= 0 && nc < g_board_size->getValue() && m_board[nr][nc] == g_human_player->getValue()) 
            {
                count++;
            }
        }
        // 威胁分数累加
        threat += count; 
    }
    return threat;
}

bool AiGame::isNearOccupied(int r, int c)
{
    const int directions[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}
    };
    for (auto& dir : directions) 
    {
        int nr = r + dir[0], nc = c + dir[1];
        if (nr >= 0 && nr < g_board_size->getValue() && nc >= 0 && nc < g_board_size->getValue() && m_board[nr][nc] != "empty")
        {
            // 该空位靠近已有棋子
            return true; 
        }
    }
    return false;
}

std::pair<int, int> AiGame::getBestMove()
{
    std::pair<int, int> bestMove = {-1, -1}; // 最佳落子位置
    int maxThreat = -1;                      // 记录最大的威胁分数

    // 1. 优先尝试进攻获胜或阻止玩家获胜
    for (int r = 0; r < g_board_size->getValue(); r++) 
    {
        for (int c = 0; c < g_board_size->getValue(); c++) 
        {
            if (m_board[r][c] != "empty") continue; // 确保当前位置为空闲

            // 模拟 AI 落子，判断是否可以获胜
            m_board[r][c] = g_ai_player->getValue();
            if (checkWin(r, c, g_ai_player->getValue())) 
            {
                // board_[r][c] = AI_PLAYER; // 恢复棋盘
                return {r, c};      // 立即获胜
            }
            m_board[r][c] = "empty";

            // 模拟玩家落子，判断是否需要防守
            m_board[r][c] = g_human_player->getValue();
            if (checkWin(r, c, g_human_player->getValue())) 
            {
                m_board[r][c] = g_ai_player->getValue(); // 恢复棋盘
                return {r, c};      // 立即防守
            }
            m_board[r][c] = "empty";
        }
    }

    // 2. 评估每个空位的威胁程度，选择最佳防守位置
    for (int r = 0; r < g_board_size->getValue(); r++) 
    {
        for (int c = 0; c < g_board_size->getValue(); c++) 
        {
            if (m_board[r][c] != "empty") 
            {
                continue; // 确保当前位置为空闲
            }
            // 评估威胁程度
            int threatLevel = evaluateThreat(r, c); 
            if (threatLevel > maxThreat) 
            {
                maxThreat = threatLevel;
                bestMove = {r, c};
            }
        }
    }

    // 3. 如果找不到威胁点，选择靠近玩家或已有棋子的空位
    if (bestMove.first == -1) 
    {
        std::vector<std::pair<int, int>> nearCells;

        for (int r = 0; r < g_board_size->getValue(); r++) 
        {
            for (int c = 0; c < g_board_size->getValue(); c++) 
            {
                if (m_board[r][c] == "empty" && isNearOccupied(r, c)) 
                { // 确保当前位置为空闲且靠近已有棋子
                    nearCells.push_back({r, c});
                }
            }
        }

        // 如果找到靠近已有棋子的空位，随机选择一个
        if (!nearCells.empty()) 
		{
            int num = rand();
			m_board[nearCells[num % nearCells.size()].first][nearCells[num % nearCells.size()].second] = g_ai_player->getValue();
            return nearCells[num % nearCells.size()];
        }

        // 4. 如果所有策略都无效，选择第一个空位（保证 AI 落子）
        for (int r = 0; r < g_board_size->getValue(); r++) 
        {
            for (int c = 0; c < g_board_size->getValue(); c++) 
            {
                if (m_board[r][c] == "empty") 
				{
					m_board[r][c] = g_ai_player->getValue();
                    return {r, c}; // 返回第一个空位
                }
            }
        }
    }
	
	m_board[bestMove.first][bestMove.second] = g_ai_player->getValue();
    return bestMove; // 返回最佳防守点或其他策略的结果
}
