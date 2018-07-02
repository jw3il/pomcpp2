#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <stack>

#include "bboard.hpp"
#include "colors.hpp"

namespace bboard
{

void State::PlantBomb(int id, int x, int y)
{
    if(agents[id].bombCount >= agents[id].maxBombCount)
    {
        return;
    }

    Bomb* b = &bombQueue.NextPos();
    b->id = id;
    b->position.x = x;
    b->position.y = y;
    b->strength = agents[id].bombStrength;
    b->velocity = Direction::IDLE;
    b->timeLeft = BOMB_LIFETIME;

    agents[id].bombCount++;
    bombQueue.bombsOnBoard++;
}

void State::PutAgent(int agentID, int x, int y)
{
    int b = Item::AGENT0 + agentID;
    board[y][x] = b;

    agents[agentID].x = x;
    agents[agentID].y = y;
}

void State::PutAgentsInCorners(int a0, int a1, int a2, int a3)
{
    int b = Item::AGENT0;

    board[0][0] = b + a0;
    board[0][BOARD_SIZE - 1] = b + a1;
    board[BOARD_SIZE - 1][BOARD_SIZE - 1] = b + a2;
    board[BOARD_SIZE - 1][0] = b + a3;

    agents[a1].x = agents[a2].x = BOARD_SIZE - 1;
    agents[a2].y = agents[a3].y = BOARD_SIZE - 1;
}

State* InitEmpty(int a0, int a1, int a2, int a3)
{
    State* result = new State();
    result->PutAgentsInCorners(a0, a1, a2, a3);
    return result;
}

State* InitState(int a0, int a1, int a2, int a3)
{
    State* result = new State();

    // Randomly put obstacles
    std::mt19937_64 rng(0x1337);
    std::uniform_int_distribution<int> intDist(0,6);
    for(int i = 0; i < BOARD_SIZE; i++)
    {
        for(int  j = 0; j < BOARD_SIZE; j++)
        {
            int tmp = intDist(rng);
            result->board[i][j] = tmp > 2 ? 0 : tmp;
        }
    }

    result->PutAgentsInCorners(a0, a1, a2, a3);
    return result;
}


void StartGame(State* state, Agent* agents[AGENT_COUNT], int timeSteps)
{
    Move moves[4];

    for(int i = 0; i < timeSteps; i++)
    {
        std::cout << "\033c"; // clear console on linux
        for(int j = 0; j < AGENT_COUNT; j++)
        {
            moves[j] = agents[j]->act(state);
        }

        Step(state, moves);
        PrintState(state);

        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
}

void PrintState(State* state)
{
    std::string result = "";

    for(int i = 0; i < BOARD_SIZE; i++)
    {
        for(int j = 0; j < BOARD_SIZE; j++)
        {
            int item = state->board[i][j];
            result += PrintItem(item);
            if(j == BOARD_SIZE - 1)
            {
                result += "\n";
            }
        }
    }
    std::cout << result << std::endl << std::endl;

    //Agent info
    for(int _ = 0; _ < AGENT_COUNT; _++)
    {
        int i = _;
        std::printf("A%d: %s %d  %s %d  %s %d\n",
                    i,
                    PrintItem(Item::EXTRABOMB).c_str(),state->agents[i].maxBombCount,
                    PrintItem(Item::INCRRANGE).c_str(),state->agents[i].bombStrength,
                    PrintItem(Item::KICK).c_str(),state->agents[i].canKick);
    }
    std::cout << "\nBombQueue\n[ ";
    for(int i = 0; i < state->bombQueue.bombsOnBoard; i++)
    {
        std::cout << state->bombQueue[i].id << " ";
    }
    std::cout << "]\n";
    //?
}

std::string PrintItem(int item)
{
    switch(item)
    {
        case Item::PASSAGE:
            return "   ";
        case Item::RIGID:
            return "[X]";
        case Item::WOOD:
            return FBLU("[\u25A0]");
        case Item::BOMB:
            return " \u25CF ";
        case Item::FLAMES:
            return " \U0001f525 ";
        case Item::EXTRABOMB:
            return " \u24B7 ";
        case Item::INCRRANGE:
            return " \u24C7 ";
        case Item::KICK:
            return " \u24C0 ";
    }
    //agent number
    if(item >= Item::AGENT0)
    {
        return " "  +  std::to_string(item - 10) + " ";
    }
    else
    {
        return "[?]";
    }
}

}

