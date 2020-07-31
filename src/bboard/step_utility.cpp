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

Position AgentBombChainReversion(State& state, Move moves[AGENT_COUNT],
                                 Position destBombs[MAX_BOMBS], int agentID)
{
    AgentInfo& agent = state.agents[agentID];
    Position origin = OriginPosition(agent.x, agent.y, moves[agentID]);

    if(!IsOutOfBounds(origin))
    {
        int indexOriginAgent = state.GetAgent(origin.x, origin.y);

        int bombDestIndex = -1;
        for(int i = 0; i < state.bombs.count; i++)
        {
            if(destBombs[i] == origin)
            {
                bombDestIndex = i;
                break;
            }
        }

        bool hasBomb  = bombDestIndex != -1;

        agent.x = origin.x;
        agent.y = origin.y;

        state[origin] = Item::AGENT0 + agentID;

        if(indexOriginAgent != -1)
        {
            return AgentBombChainReversion(state, moves, destBombs, indexOriginAgent);
        }
        // move bomb back and check for an agent that needs to be reverted
        else if(hasBomb)
        {
            Bomb& b = state.bombs[bombDestIndex];
            Position bombDest = destBombs[bombDestIndex];

            Position originBomb = OriginPosition(bombDest.x, bombDest.y, Move(BMB_DIR(b)));

            // this is the case when an agent gets bounced back to a bomb he laid
            if(originBomb == bombDest)
            {
                state[originBomb] = Item::AGENT0 + agentID;
                return originBomb;
            }

            int hasAgent = state.GetAgent(originBomb.x, originBomb.y);
            SetBombDirection(b, Direction::IDLE);
            SetBombPosition(b, originBomb.x, originBomb.y);
            state[originBomb] = Item::BOMB;

            if(hasAgent != -1)
            {
                return AgentBombChainReversion(state, moves, destBombs, hasAgent);
            }
            else
            {
                return originBomb;
            }
        }
        return origin;
    }
    else
    {
        return {agent.x, agent.y};
    }
}

void FillPositions(State* s, Position p[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        p[i] = {s->agents[i].x, s->agents[i].y};
    }
}

void FillDestPos(State* s, Move m[AGENT_COUNT], Position p[AGENT_COUNT])
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        p[i] = DesiredPosition(s->agents[i].x, s->agents[i].y, m[i]);
    }
}

void FillBombDestPos(State* s, Position p[MAX_BOMBS])
{
    for(int i = 0; i < s->bombs.count; i++)
    {
        p[i] = DesiredPosition(s->bombs[i]);
    }
}

void FixDestPos(State* s, Position d[AGENT_COUNT])
{
    bool fixDest[AGENT_COUNT];
    std::fill_n(fixDest, AGENT_COUNT, false);

    for(int i = 0; i < AGENT_COUNT; i++)
    {
        // skip dead agents
        if (s->agents[i].dead)
            continue;

        for(int j = i + 1; j < AGENT_COUNT; j++)
        {
            // skip dead agents
            if (s->agents[j].dead)
                continue;

            // forbid moving to the same position and switching positions
            if(d[i] == d[j] || (d[i].x == s->agents[j].x && d[i].y == s->agents[j].y &&
                    d[j].x == s->agents[i].x && d[j].y == s->agents[i].y))
            {
                fixDest[i] = true;
                fixDest[j] = true;
            }
        }
    }

    for (int i = 0; i < AGENT_COUNT; i++) {
        if(fixDest[i]) {
            d[i].x = s->agents[i].x;
            d[i].y = s->agents[i].y;
        }
    }
}

int ResolveDependencies(State* s, Position des[AGENT_COUNT],
                        int dependency[AGENT_COUNT], int chain[AGENT_COUNT])
{
    int rootCount = 0;
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        // dead agents are handled as roots
        if(s->agents[i].dead)
        {
            chain[rootCount] = i;
            rootCount++;
            continue;
        }

        bool isChainRoot = true;
        for(int j = 0; j < AGENT_COUNT; j++)
        {
            if(i == j || s->agents[j].dead) continue;

            if(des[i].x == s->agents[j].x && des[i].y == s->agents[j].y)
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


void TickFlames(State& state)
{
    for(int i = 0; i < state.flames.count; i++)
    {
        state.flames[i].timeLeft--;
    }
    int flameCount = state.flames.count;
    for(int i = 0; i < flameCount; i++)
    {
        if(state.flames[0].timeLeft == 0)
        {
            state.PopFlame();
        }
    }
}

void TickBombs(State& state)
{
    for(int i = 0; i < state.bombs.count; i++)
    {
        ReduceBombTimer(state.bombs[i]);
    }

    //explode timed-out bombs
    int bombCount = state.bombs.count;
    for(int i = 0; i < bombCount && state.bombs.count > 0; i++)
    {
        if(BMB_TIME(state.bombs[0]) == 0)
        {
            state.ExplodeTopBomb();
        }
        else
        {
            break;
        }

    }
}

void ConsumePowerup(State& state, int agentID, int powerUp)
{
    if(powerUp == Item::EXTRABOMB)
    {
        state.agents[agentID].maxBombCount++;
    }
    else if(powerUp == Item::INCRRANGE)
    {
        state.agents[agentID].bombStrength++;
    }
    else if(powerUp == Item::KICK)
    {
        state.agents[agentID].canKick = true;
    }

}

bool HasDPCollision(const State& state, Position dp[AGENT_COUNT], int agentID)
{
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        if(agentID == i || state.agents[i].dead) continue;
        if(dp[agentID] == dp[i])
        {
            // a destination position conflict will never
            // result in a valid move
            return true;
        }
    }
    return false;
}

bool HasBombCollision(const State& state, const Bomb& b, int index)
{
    Position bmbTarget = util::DesiredPosition(b);

    for(int i = index; i < state.bombs.count; i++)
    {
        Position target = util::DesiredPosition(state.bombs[i]);

        if(b != state.bombs[i] && target == bmbTarget)
        {
            return true;
        }
    }
    return false;
}

void ResolveBombCollision(State& state, Move moves[AGENT_COUNT],
                          Position destBombs[MAX_BOMBS], int index)
{
    Bomb& b = state.bombs[index];
    Bomb collidees[4]; //more than 4 bombs cannot collide
    Position bmbTarget = util::DesiredPosition(b);
    bool hasCollided = false;

    for(int i = index; i < state.bombs.count; i++)
    {
        Position target = util::DesiredPosition(state.bombs[i]);

        if(b != state.bombs[i] && target == bmbTarget)
        {
            SetBombDirection(state.bombs[i], Direction::IDLE);
            hasCollided = true;
        }
    }
    if(hasCollided)
    {
        if(Direction(BMB_DIR(b)) != Direction::IDLE)
        {
            SetBombDirection(b, Direction::IDLE);
            int index = state.GetAgent(BMB_POS_X(b), BMB_POS_Y(b));
            // move != idle means the agent moved on it this turn
            if(index > -1 && moves[index] != Move::IDLE && moves[index] != Move::BOMB)
            {
                Position origin = AgentBombChainReversion(state, moves, destBombs, index);
                state.board[BMB_POS_Y(b)][BMB_POS_X(b)] = Item::BOMB;
            }

        }
    }

}

void ResetBombFlags(State& state)
{
    for(int i = 0; i < state.bombs.count; i++)
    {
        SetBombMovedFlag(state.bombs[i], false);
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
    int winningTeam = 0;
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
                winningTeam = info.team;
                // if not, that is the winning agent
                if(winningTeam == 0)
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
        winningTeam = util::GetWinningTeam(state);
    }

    // all agents in the winning team have won
    if(winningTeam != 0)
    {
        state.finished = true;
        state.isDraw = false;

        for(int i = 0; i < AGENT_COUNT; i++)
        {
            AgentInfo& info = state.agents[i];
            if (info.team == winningTeam)
            {
                info.won = true;
            }
            else
            {
                info.won = false;
            }
        }
    }

    state.winningTeam = winningTeam;
}


}
