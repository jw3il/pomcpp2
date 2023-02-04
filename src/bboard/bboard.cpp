#include <random>
#include <chrono>
#include <thread>
#include <iostream>

#include "bboard.hpp"
#include "colors.hpp"

namespace bboard
{

/////////////////////////
// Auxiliary Functions //
/////////////////////////

/**
 * @brief IsOutOfBounds Checks wether a given position is out of bounds
 */
inline bool IsOutOfBounds(const int& x, const int& y)
{
    return x < 0 || y < 0 || x >= BOARD_SIZE || y >= BOARD_SIZE;
}

void SetTeams(AgentInfo agents[AGENT_COUNT], GameMode gameMode)
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        agents[i].team = GetTeam(gameMode, i);
    }
}

///////////////////
// Board Methods //
///////////////////

void Board::CopyFrom(const Board& board, bool copyAgents)
{
    std::copy_n(&board.items[0][0], BOARD_SIZE * BOARD_SIZE, &items[0][0]);
    
    bombs = board.bombs;
    flames = board.flames;
    timeStep = board.timeStep;
    currentFlameTime = board.currentFlameTime;

    if(copyAgents)
    {
        for(uint i = 0; i < AGENT_COUNT; i++)
        {
            agents[i] = board.agents[i];
        }
    }
}

int Board::GetAgent(int x, int y) const
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(!agents[i].dead && agents[i].x == x && agents[i].y == y)
        {
            return i;
        }
    }
    return -1;
}

void Board::PutAgent(int x, int y, int agentID)
{
    items[y][x] = Item::AGENT0 + agentID;

    agents[agentID].x = x;
    agents[agentID].y = y;
}

void Board::PutAgentsInCorners(int a0, int a1, int a2, int a3, int padding)
{
    const int min = padding;
    const int max = BOARD_SIZE - (1 + padding);

    PutAgent(min, min, a0);
    PutAgent(max, min, a1);
    PutAgent(max, max, a2);
    PutAgent(min, max, a3);
}

void Board::PutBomb(int x, int y, int agentID, int strength, int lifeTime, bool setItem)
{
    Bomb& b = bombs.NextPos();

    SetBombID(b, agentID);
    SetBombPosition(b, x, y);
    SetBombStrength(b, strength);
    SetBombDirection(b, Direction::IDLE);
    SetBombFlag(b, false);
    SetBombTime(b, lifeTime);

    if(setItem)
    {
        items[y][x] = Item::BOMB;
    }

    bombs.count++;
    agents[agentID].bombCount++;
}

bool Board::HasBomb(int x, int y) const
{
    for(int i = 0; i < bombs.count; i++)
    {
        if(BMB_POS_X(bombs[i]) == x && BMB_POS_Y(bombs[i]) == y)
        {
            return true;
        }
    }
    return false;
}

Bomb* Board::GetBomb(int x, int y) const
{
    for(int i = 0; i < bombs.count; i++)
    {
        if(BMB_POS_X(bombs[i]) == x && BMB_POS_Y(bombs[i]) == y)
        {
            return (Bomb*)&bombs[i];
        }
    }
    return nullptr;
}

int Board::GetBombIndex(int x, int y) const
{
    for(int i = 0; i < bombs.count; i++)
    {
        if(BMB_POS_X(bombs[i]) == x && BMB_POS_Y(bombs[i]) == y)
        {
            return i;
        }
    }
    return -1;
}

Item Board::FlagItem(int pwp)
{
    switch (pwp) {
        case 1: return Item::EXTRABOMB;
        case 2: return Item::INCRRANGE;
        case 3: return Item::KICK;
        default: return Item::PASSAGE;
    }
}

int Board::ItemFlag(Item item)
{
    switch (item) {
        case Item::EXTRABOMB: return 1;
        case Item::INCRRANGE: return 2;
        case Item::KICK: return 3;
        default: return 0;
    }
}

void Board::Print(bool clearConsole) const
{
    // clears console on linux
    if(clearConsole)
        std::cout << "\033c";

    for(int y = 0; y < BOARD_SIZE; y++)
    {
        for(int x = 0; x < BOARD_SIZE; x++)
        {
            int item = items[y][x];
            std::cout << PrintItem(item);
        }
        std::cout << std::endl;
    }
}

///////////////////
// State Methods //
///////////////////


//////////////////////
// bboard namespace //
//////////////////////

void bboard::Agent::reset()
{
    // default reset does nothing
}

template <typename T, int c>
void _printArray(const T arr[c])
{
    std::cout << "[";

    for(int i = 0; i < c - 1; i++)
    {
        std::cout << arr[i] << ", ";
    }

    if(c > 0)
    {
        std::cout << arr[c-1];
    }

    std::cout << "]";
}

std::string PrintItem(int item)
{
    switch(item)
    {
        case Item::PASSAGE:
            return "   ";
        case Item::RIGID:
            return "[X]";
        case Item::BOMB:
            return " \u25CF ";
        case Item::EXTRABOMB:
            return " \u24B7 ";
        case Item::INCRRANGE:
            return " \u24C7 ";
        case Item::KICK:
            return " \u24C0 ";
        case Item::FOG:
            return "[@]";
    }

    if(IS_WOOD(item))
    {
        return FBLU((std::string)"[\u25A0]");
    }

    if(IS_FLAME(item))
    {
        return FRED((std::string)" \U0000263C ");
    }

    //agent number
    if(item >= Item::AGENT0)
    {
        return " "  +  std::to_string(item - Item::AGENT0) + " ";
    }
    else
    {
        // unknown item
        return "[?]";
    }
}

}

