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

Position AgentBombChainReversion(State* state, const Position oldAgentPos[AGENT_COUNT],
                                 Position destBombs[MAX_BOMBS], int agentID)
{
    // scenario: An agent shares its position with a bomb
    // -> we have to reset the agent to its original position (could be staying on a bomb)
    //    and check whether resetting the agent affects any additional agents or bombs

    AgentInfo& agent = state->agents[agentID];
    Position origin = oldAgentPos[agentID];

    // is there an agent at the old origin?
    int indexOriginAgent = state->GetAgent(origin.x, origin.y);

    // reset agent to its original position
    agent.x = origin.x;
    agent.y = origin.y;

    int bombDestIndex = -1;
    if(indexOriginAgent != -1)
    {
        // we also have to move back the agent which moved to our origin position
        AgentBombChainReversion(state, oldAgentPos, destBombs, indexOriginAgent);

        // is there a bomb which has been kicked by this agent just in this step?
        // if that's the case, undo the kick
        const AgentInfo& originAgent = state->agents[indexOriginAgent];
        if(originAgent.canKick)
        {
            for(int i = 0; i < state->bombs.count; i++)
            {
                // kicked bombs will still be located at the origin
                // because they did not move yet
                if(BMB_POS(state->bombs[i]) == origin)
                {
                    bombDestIndex = i;
                    break;
                }
            }
        }
    }
    else
    {
        // is there a bomb which wants to move to / stay at the old origin?
        for(int i = 0; i < state->bombs.count; i++)
        {
            if(destBombs[i] == origin)
            {
                bombDestIndex = i;
                break;
            }
        }
    }

    // move bomb back and check for an agent that needs to be reverted
    if(bombDestIndex != -1)
    {
        Bomb& b = state->bombs[bombDestIndex];

        // the bomb did not move. As we've already set the origin, we are done
        // this is the case when an agent gets bounced back to a bomb he laid
        // and the bomb as not been kicked
        if(Move(BMB_DIR(b)) == Move::IDLE)
        {
            return origin;
        }

        // otherwise: the bomb wanted to move. Reset it
        Position bPos = BMB_POS(b);

        // check whether there is now an agent at the old position of the bomb
        int hasAgent = state->GetAgent(bPos.x, bPos.y);

        // reset the bomb
        SetBombDirection(b, Direction::IDLE);
        SetBombPosition(b, bPos);
        destBombs[bombDestIndex] = bPos;

        if(hasAgent == agentID)
        {
            // the agent moved back on its own bomb (which might have been
            // kicked by other agents, so we had to revert it to idle)
            return bPos;
        }

        // otherwise, bPos is either empty or occupied by some different agent.
        // as we'll move back the other agent, we can already set the bomb item
        state->items[bPos.y][bPos.x] = Item::BOMB;

        if(hasAgent != -1)
        {
            // if there is an agent, move it back as well
            return AgentBombChainReversion(state, oldAgentPos, destBombs, hasAgent);
        }
        else
        {
            return bPos;
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
        p[i] = BMB_POS(board->bombs[i]);
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

        // dead and not visible agents are handled as roots
        if(a1.dead || !a1.visible)
        {
            chain[rootCount] = i;
            rootCount++;
            continue;
        }

        bool isChainRoot = true;
        for(int j = 0; j < AGENT_COUNT; j++)
        {
            const AgentInfo& a2 = state->agents[j];

            if(i == j || a2.dead || !a2.visible) continue;

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

void TickFlames(State* state)
{
    if(state->flames.count == 0)
    {
        return;
    }

    if(state->currentFlameTime == -1)
    {
        throw std::runtime_error("TickFlames only supports optimized flame queues.");
    }

    state->currentFlameTime--;
    state->flames[0].timeLeft--;
    if(state->flames[0].timeLeft <= 0)
    {
        state->PopFlames();
    }
}

void TickBombs(State* state)
{
    // reduce timer of every bomb
    for(int i = 0; i < state->bombs.count; i++)
    {
        ReduceBombTimer(state->bombs[i]);
    }
}

void ExplodeBombs(State* state)
{
    // always check the current top bomb
    while (state->bombs.count > 0 && BMB_TIME(state->bombs[0]) <= 0)
    {
        state->ExplodeBombAt(0);
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

void ResetBombFlags(Board* board)
{
    for(int i = 0; i < board->bombs.count; i++)
    {
        SetBombFlag(board->bombs[i], false);
    }
}

inline Direction _toDirection(Position difference)
{
    if(difference.x != 0)
    {
        if(difference.x > 0) 
        {
            return Direction::RIGHT;
        }
        else
        {
            return Direction::LEFT;
        }
    }
    if(difference.y != 0)
    {
        if(difference.y > 0) 
        {
            return Direction::DOWN;
        }
        else
        {
            return Direction::UP;
        }
    }
    return Direction::IDLE;
}

void ResolveBombMovement(State* state, const Position oldAgentPos[AGENT_COUNT], const Position originalAgentDestination[AGENT_COUNT], Position bombDestinations[])
{
    // Fill array of desired positions
    const int bombCount = state->bombs.count;
    Position bombPositions[bombCount];
    util::FillBombPositions(state, bombPositions);
    
    bool stoppingBombs[bombCount];
    std::fill_n(stoppingBombs, bombCount, false);
    bool foundStoppingBomb = false;

    bool agentCollisions[AGENT_COUNT];
    std::fill_n(agentCollisions, AGENT_COUNT, false);
    bool foundAgentCollision = false;

    // agent-bomb collisions
    for(int i = 0; i < state->bombs.count; i++)
    {
        const Position& bombDestination = bombDestinations[i];
        if(util::BombMovementIsBlocked(state, bombDestination))
        {
            // bombs can be kicked
            int agentid = state->GetAgent(bombDestination.x, bombDestination.y);
            if(agentid != -1)
            {
                const AgentInfo &info = state->agents[agentid];

                if(info.canKick) 
                {
                    const Position diff = info.GetPos() - oldAgentPos[agentid];
                    const Position newDestination = info.GetPos() + diff;
                    if(util::BombMovementIsBlocked(state, newDestination))
                    {
                        // we cannot kick the bomb
                        // .. we have to check if we have to bounce back an agent at the kick destination
                        int indexAgent = state->GetAgent(newDestination.x, newDestination.y);
                        if(indexAgent > -1 && state->agents[indexAgent].GetPos() != oldAgentPos[indexAgent])
                        {
                            agentCollisions[indexAgent] = true;
                            foundAgentCollision = true;
                        }
                        // and also bounce back the agent at the original destination
                        agentCollisions[agentid] = true;
                        foundAgentCollision = true;
                    }
                    else 
                    {
                        bool kickingAllowed = true;
                        // kicking is not allowed if a moving bomb blocks the destination
                        for(int j = 0; j < state->bombs.count; j++)
                        {
                            if(i == j) continue;

                            if(bombDestinations[j] == newDestination)
                            {
                                kickingAllowed = false;
                                break;
                            }
                        }

                        if(kickingAllowed)
                        {
                            SetBombDirection(state->bombs[i], _toDirection(diff));
                            bombDestinations[i] = newDestination;
                            continue;
                        }
                        // note that this is treated as a collision if kicking is not allowed
                        // to reset the agents position
                    }
                }
                else
                {
                    // agent at destination cannot kick and collides with the bomb => undo movement
                    agentCollisions[agentid] = true;
                    foundAgentCollision = true;
                }
            }
            foundStoppingBomb = true;
            stoppingBombs[i] = true;
            bombDestinations[i] = bombPositions[i];
        }
        // else: bomb movement is not blocked
        else if(bombDestination != bombPositions[i])
        {
            // check that moving bomb does not overlap with agent collision
            for(int a = 0; a < AGENT_COUNT; a++)
            {
                // agent wanted to move but somehow did not get there (otherwise bomb movement would have been blocked)
                // => bomb is not allowed to move either
                if(originalAgentDestination[a] == bombDestination)
                {
                    foundStoppingBomb = true;
                    stoppingBombs[i] = true;
                    bombDestinations[i] = bombPositions[i];
                    break;
                }
            }
        }
    }

    // bomb-bomb collisions
    bool res = util::FixDestPos<false>(bombPositions, bombDestinations, stoppingBombs, state->bombs.count);
    foundStoppingBomb = foundStoppingBomb || res;

    if(foundAgentCollision)
    {
        for(int i = 0; i < AGENT_COUNT; i++)
        {
            Position p = state->agents[i].GetPos();
            // only revert agents that moved
            if(agentCollisions[i] && oldAgentPos[i] != p)
            {
                util::AgentBombChainReversion(state, oldAgentPos, bombDestinations, i);
            }
        }
    }

    if(foundStoppingBomb)
    {
        for(int i = 0; i < state->bombs.count; i++)
        {
            if(!stoppingBombs[i])
                continue;

            // bombs with collisions should not move
            Bomb& b = state->bombs[i];
            SetBombDirection(b, Direction::IDLE);

            Position bPos = bombPositions[i];

            // this bomb had some collision, check whether we have to continue the chain
            int indexAgent = state->GetAgent(bPos.x, bPos.y);
            if(indexAgent > -1
                    // the agent has moved
                    && state->agents[indexAgent].GetPos() != oldAgentPos[indexAgent])
            {
                // bounce back the moving agent to its origin
                util::AgentBombChainReversion(state, oldAgentPos, bombDestinations, indexAgent);
                if(state->GetAgent(bPos.x, bPos.y) == -1)
                {
                    // this position is now occupied by the bomb
                    state->items[bPos.y][bPos.x] = Item::BOMB;
                }
            }
        }
    }
}

inline void _resetBoardAgentGone(Board* board, const int x, const int y, const int i)
{
    if(board->items[y][x] == Item::AGENT0 + i)
    {
        if(board->HasBomb(x, y))
        {
            board->items[y][x] = Item::BOMB;
        }
        else
        {
            board->items[y][x] = Item::PASSAGE;
        }
    }
}

inline void _setAgentPos(State* state, const int x, const int y, const int i)
{
    state->agents[i].x = x;
    state->agents[i].y = y;
}

void MoveAgent(State* state, const int i, const Move m, const Position fixedDest, const bool ouroboros)
{
    AgentInfo& a = state->agents[i];

    // hide agent from board and assume he does not move
    _resetBoardAgentGone(state, a.x, a.y, i);
    _setAgentPos(state, a.x, a.y, i);

    if(a.dead || !a.visible)
    {
        return;
    }
    else if(m == Move::BOMB)
    {
        state->TryPutBomb<true>(i);
        return;
    }
    else if(m == Move::IDLE || fixedDest == a.GetPos())
    {
        // important: has to be after m == bomb because we won't move when
        return;
    }

    // the agent wants to move
    int itemOnDestination = state->items[fixedDest.y][fixedDest.x];
    if(util::IsOutOfBounds(fixedDest) || IS_WOOD(itemOnDestination) || itemOnDestination == Item::RIGID)
    {
        // cannot walk out of bounds, on wooden and rigid boxes
        return;
    }
    if(!ouroboros && state->GetAgent(fixedDest.x, fixedDest.y) != -1)
    {
        // cannot walk on agents that collided with something (chain order)
        return;
    }

    //int itemOnDestination = state->items[fixedDest.y][fixedDest.x];
    //if(itemOnDestination == Item::PASSAGE || IS_FLAME(itemOnDestination) || IS_POWERUP(itemOnDestination) || state->HasBomb(fixedDest.x, fixedDest.y) || ouroboros)
    //{
    // only allow moving when possible, bomb collisions are resolved later
    _setAgentPos(state, fixedDest.x, fixedDest.y, i);
    //}
}

void MoveBombs(State* state, const Position bombDestinations[])
{
    // Reset bomb exploded flags
    util::ResetBombFlags(state);

    // Move bombs (no collision handling anymore)
    bool bombExplodedDuringMovement = false;
    for(int i = 0; i < state->bombs.count; i++)
    {
        Bomb& b = state->bombs[i];
        Position pos = BMB_POS(b);
        Position dest = bombDestinations[i];

        if(pos == dest)
        {
            // this bomb does not move
            continue;
        }

        int& oItem = state->items[pos.y][pos.x];
        int& tItem = state->items[dest.y][dest.x];

        if(util::IsOutOfBounds(dest) || IS_STATIC_MOV_BLOCK(tItem))
        {
            // stop moving the bomb
            SetBombDirection(b, Direction::IDLE);
        }
        else if(tItem == Item::FOG)
        {
            // the bomb just disappears when it moves out of range
            if(oItem == Item::BOMB)
            {
                oItem = Item::PASSAGE;
            }
            state->bombs.RemoveAt(i);
            i--;
        }
        else
        {
            // move bomb
            SetBombPosition(b, dest.x, dest.y);

            if(!state->HasBomb(pos.x, pos.y) && oItem == Item::BOMB)
            {
                oItem = Item::PASSAGE;
            }

            if(IS_WALKABLE(tItem))
            {
                tItem = Item::BOMB;
            }
            else if(IS_FLAME(tItem))
            {
                // bomb moved into flame -> explode later
                bombExplodedDuringMovement = true;
                SetBombFlag(b, true);
            }
        }
    }

    if(bombExplodedDuringMovement)
    {
        for(int i = 0; i < state->bombs.count; i++)
        {
            if(BMB_FLAG(state->bombs[i]) == int(true))
            {
                state->ExplodeBombAt(i);
                i--;
            }
        }
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
    state.finished = false;
    state.isDraw = false;
    state.winningAgent = -1;
    state.winningTeam = 0;

    if(state.aliveAgents == 0)
    {
        // nobody won when all agents are dead
        state.finished = true;
        state.isDraw = true;
    }
    else if(state.aliveAgents == 1)
    {
        // a single agent won the game (?)

        state.finished = true;
        state.isDraw = false;

        for(int i = 0; i < AGENT_COUNT; i++)
        {
            AgentInfo& info = state.agents[i];
            if (!info.dead)
            {
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
    }
}

bool CompareTimeLeft(const Flame& lhs, const Flame& rhs)
{
    return lhs.timeLeft < rhs.timeLeft;
}

int OptimizeFlameQueue(Board& board)
{
    if(board.currentFlameTime != -1)
    {
        return board.currentFlameTime;
    }

    // sort flames
    Flame flames[board.flames.count];
    board.flames.CopyTo(flames);
    std::sort(flames, flames + board.flames.count, CompareTimeLeft);
    board.flames.CopyFrom(flames, board.flames.count);

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
