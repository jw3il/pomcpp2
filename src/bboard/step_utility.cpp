#include <iostream>

#include "bboard.hpp"
#include "step_utility.hpp"

namespace bboard::util
{

Position DesiredPosition(int x, int y, Move move)
{
    Position p;
    p.x = x;
    p.y = y;
    if(move == Move::UP)
    {
        p.y -= 1;
    }
    else if(move == Move::DOWN)
    {
        p.y += 1;
    }
    else if(move == Move::LEFT)
    {
        p.x -= 1;
    }
    else if(move == Move::RIGHT)
    {
        p.x += 1;
    }
    return p;
}

Position OriginPosition(int x, int y, Move move)
{
    Position p;
    p.x = x;
    p.y = y;
    if(move == Move::DOWN)
    {
        p.y -= 1;
    }
    else if(move == Move::UP)
    {
        p.y += 1;
    }
    else if(move == Move::RIGHT)
    {
        p.x -= 1;
    }
    else if(move == Move::LEFT)
    {
        p.x += 1;
    }
    return p;
}

Position DesiredPosition(const Bomb b)
{
    return DesiredPosition(BMB_POS_X(b), BMB_POS_Y(b), Move(BMB_DIR(b)));
}

Position AgentBombChainReversion(State* state, Position oldAgentPos[AGENT_COUNT], Move moves[AGENT_COUNT],
                                 Position destBombs[MAX_BOMBS], int agentID)
{    
    AgentInfo& agent = state->agents[agentID];
    Position origin = oldAgentPos[agentID];

    // is there an agent at the old origin?
    int indexOriginAgent = state->GetAgent(origin.x, origin.y);

    // reset agent to its original position
    agent.x = origin.x;
    agent.y = origin.y;
    state->items[origin.y][origin.x] = Item::AGENT0 + agentID;

    if(indexOriginAgent != -1)
    {
        // we also have to move back the agent which moved to our origin position
        // TODO: Is the return here correct?
        return AgentBombChainReversion(state, oldAgentPos, moves, destBombs, indexOriginAgent);
    }

    // is there a bomb which wants to move to / stay at the old origin?
    int bombDestIndex = -1;
    for(int i = 0; i < state->bombs.count; i++)
    {
        if(destBombs[i] == origin)
        {
            bombDestIndex = i;
            break;
        }
    }

    // move bomb back and check for an agent that needs to be reverted
    if(bombDestIndex != -1)
    {
        Bomb& b = state->bombs[bombDestIndex];

        // the bomb did not move. As we've already set the origin, we are done
        // this is the case when an agent gets bounced back to a bomb he laid
        if(Move(BMB_DIR(b)) == Move::IDLE)
        {
            return origin;
        }

        // otherwise: the bomb wanted to move. Reset it
        int bX = BMB_POS_X(b);
        int bY = BMB_POS_Y(b);

        // check whether there is now an agent at the old position of the bomb
        int hasAgent = state->GetAgent(bX, bY);

        // reset the bomb
        SetBombDirection(b, Direction::IDLE);
        SetBombPosition(b, bX, bY);
        destBombs[bombDestIndex] = {bX, bY};
        state->items[bX][bY] = Item::BOMB;

        if(hasAgent != -1)
        {
            // if there is an agent, move it back as well
            return AgentBombChainReversion(state, oldAgentPos, moves, destBombs, hasAgent);
        }
        else
        {
            return {bX, bY};
        }
    }

    return origin;
}

void FillPositions(const State* state, Position p[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        p[i] = {state->agents[i].x, state->agents[i].y};
    }
}

void FillDestPos(const State* state, Move m[AGENT_COUNT], Position p[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        p[i] = DesiredPosition(state->agents[i].x, state->agents[i].y, m[i]);
    }
}

void FillBombPositions(const Board* board, Position p[])
{
    for(int i = 0; i < board->bombs.count; i++)
    {
        Bomb b = board->bombs[i];
        p[i] = {BMB_POS_X(b), BMB_POS_Y(b)};
    }
}

void FillBombDestPos(const Board* board, Position p[MAX_BOMBS])
{
    for(int i = 0; i < board->bombs.count; i++)
    {
        p[i] = DesiredPosition(board->bombs[i]);
    }
}

void FillAgentDead(const State* state, bool dead[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        dead[i] = state->agents[i].dead;
    }
}

void _printDest(Position destPos[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT - 1; i++)
    {
        std::cout << destPos[i] << ", ";
    }
    std::cout << destPos[AGENT_COUNT - 1] << std::endl;
}

int ResolveDependencies(const State* state, Position des[AGENT_COUNT],
                        int dependency[AGENT_COUNT], int chain[AGENT_COUNT])
{
    int rootCount = 0;
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        const AgentInfo& a1 = state->agents[i];

        // dead and ignored agents are handled as roots
        if(a1.dead || a1.ignore)
        {
            chain[rootCount] = i;
            rootCount++;
            continue;
        }

        bool isChainRoot = true;
        for(int j = 0; j < AGENT_COUNT; j++)
        {
            const AgentInfo& a2 = state->agents[j];

            if(i == j || a2.dead || a2.ignore) continue;

            if(des[i].x == a2.x && des[i].y == a2.y)
            {
                dependency[j] = i;
                isChainRoot = false;
                break;
            }
        }
        if(isChainRoot)
        {
            chain[rootCount] = i;
            rootCount++;
        }
    }
    return rootCount;
}

void TickFlames(Board* board)
{
    board->currentFlameTime--;
    board->flames[0].timeLeft--;
    if(board->flames[0].timeLeft <= 0)
    {
        board->PopFlames();
    }
}

void TickBombs(Board* board)
{
    // reduce timer of every bomb
    for(int i = 0; i < board->bombs.count; i++)
    {
        ReduceBombTimer(board->bombs[i]);
    }

    //explode timed-out bombs
    for(int i = 0; i < board->bombs.count; i++)
    {
        // always check the current top bomb
        if(BMB_TIME(board->bombs[0]) == 0)
        {
            board->ExplodeBombAt(0);
        }
        else
        {
            // bombs are ordered according to their
            // time -> we can already stop here
            break;
        }
    }
}

void ConsumePowerup(AgentInfo& info, int powerUp)
{
    if(powerUp == Item::EXTRABOMB)
    {
        info.maxBombCount++;
    }
    else if(powerUp == Item::INCRRANGE)
    {
        info.bombStrength++;
    }
    else if(powerUp == Item::KICK)
    {
        info.canKick = true;
    }
}

bool HasDPCollision(const State* state, Position dp[AGENT_COUNT], int agentID)
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(agentID == i || state->agents[i].dead) continue;
        if(dp[agentID] == dp[i])
        {
            // a destination position conflict will never
            // result in a valid move
            return true;
        }
    }
    return false;
}

bool ResolveBombCollision(State* state, Position oldAgentPos[AGENT_COUNT], Move moves[AGENT_COUNT],
                          Position bombDest[MAX_BOMBS], int index)
{
    Bomb b = state->bombs[index];
    bool hasCollided = false;

    for(int i = index + 1; i < state->bombs.count; i++)
    {
        if(bombDest[i] == bombDest[index])
        {
            SetBombDirection(state->bombs[i], Direction::IDLE);
            hasCollided = true;
        }
    }

    if(hasCollided)
    {
        if(Direction(BMB_DIR(b)) != Direction::IDLE)
        {
            SetBombDirection(b, Direction::IDLE);
            int index = BMB_ID(b);
            // move != idle means the agent moved on it this turn
            if(index > -1 && moves[index] != Move::IDLE && moves[index] != Move::BOMB)
            {
                AgentBombChainReversion(state, oldAgentPos, moves, bombDest, index);
                state->items[BMB_POS_Y(b)][BMB_POS_X(b)] = Item::BOMB;
            }
        }
    }

    return hasCollided;
}

void ResetBombFlags(Board* board)
{
    for(int i = 0; i < board->bombs.count; i++)
    {
        SetBombFlag(board->bombs[i], false);
    }
}

void PrintDependency(int dependency[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(dependency[i] == -1)
        {
            std::cout << "[" << i << " <- ]";
        }
        else
        {
            std::cout << "[" << i << " <- " << dependency[i] << "]";
        }
        std::cout << std::endl;
    }
}

void PrintDependencyChain(int dependency[AGENT_COUNT], int chain[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(chain[i] == -1) continue;

        std::cout << chain[i];
        int k = dependency[chain[i]];

        while(k != -1)
        {
            std::cout << " <- " << k;
            k = dependency[k];
        }
        std::cout << std::endl;
    }
}

int GetWinningTeam(const State& state)
{
    // no team has won when there are no agents left
    if(state.aliveAgents == 0)
    {
        return 0;
    }

    int winningTeamCandidate = 0;

    for(int i = 0; i < AGENT_COUNT; i++)
    {
        AgentInfo info = state.agents[i];
        if (!info.dead)
        {
            if (state.aliveAgents == 1)
            {
                // return the team of the last agent
                // - warning: could also be 0 (no team)
                return info.team;
            }

            // agent is in some team and there are > 1 alive agents
            if (info.team != 0)
            {
                if (winningTeamCandidate == 0)
                {
                    winningTeamCandidate = info.team;
                }
                else if (winningTeamCandidate == info.team)
                {
                    continue;
                }
                else
                {
                    // winning team is different than own team!
                    // -> this means there are at least two alive
                    // agents from different teams
                    return 0;
                }
            }
        }
    }

    // return the winning team
    return winningTeamCandidate;
}

void CheckTerminalState(State& state)
{
    state.winningAgent = -1;
    state.winningTeam = 0;

    if(state.aliveAgents == 0)
    {
        // nobody won when all agents are dead
        state.finished = true;
        state.isDraw = true;

        for(int i = 0; i < AGENT_COUNT; i++)
        {
            state.agents[i].won = false;
        }
    }
    else if(state.aliveAgents == 1)
    {
        // a single agent won the game (?)

        state.finished = true;
        state.isDraw = false;

        for(int i = 0; i < AGENT_COUNT; i++)
        {
            AgentInfo& info = state.agents[i];
            if (info.dead)
            {
                info.won = false;
            }
            else
            {
                info.won = true;
                // the agent might be in some team
                state.winningTeam = info.team;
                // if not, that is the winning agent
                if(state.winningTeam == 0)
                {
                    state.winningAgent = i;
                }

                break;
            }
        }
    }
    else
    {
        // there are >= 2 agents alive, check if there
        // is a winning team
        state.winningTeam = util::GetWinningTeam(state);
    }

    // all agents in the winning team have won
    if(state.winningTeam != 0)
    {
        state.finished = true;
        state.isDraw = false;

        for(int i = 0; i < AGENT_COUNT; i++)
        {
            AgentInfo& info = state.agents[i];
            if (info.team == state.winningTeam)
            {
                info.won = true;
            }
            else
            {
                info.won = false;
            }
        }
    }
}

bool CompareTimeLeft(const Flame& lhs, const Flame& rhs)
{
    return lhs.timeLeft < rhs.timeLeft;
}

int OptimizeFlameQueue(Board& board)
{
    // sort flames
    std::sort(board.flames.queue, board.flames.queue + board.flames.count, CompareTimeLeft);

    // modify timeLeft (additive)
    int timeLeft = 0;
    for(int i = 0; i < board.flames.count; i++)
    {
        Flame& f = board.flames[i];
        int oldVal = f.timeLeft;
        f.timeLeft -= timeLeft;
        timeLeft = oldVal;

        // set flame ids to allow for faster lookup
        board.items[f.position.y][f.position.x] += (i << 3);
    }

    // return total time left
    return timeLeft;
}

}
