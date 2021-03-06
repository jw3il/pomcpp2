#include "bboard.hpp"
#include "agents.hpp"
#include "strategy.hpp"
#include "step_utility.hpp"

using namespace bboard;
using namespace bboard::strategy;

namespace agents
{

void _init_axes(SimpleUnbiasedAgent& me)
{
    for (int i = 0; i < AGENT_COUNT; i++)
    {
        me.agentAxis[i] = i;
    }

    for (int i = 0; i < BOARD_SIZE; i++)
    {
        me.boardAxisX[i] = i;
        me.boardAxisY[i] = i;
    }

    me.dirAxis[0] = Move::UP;
    me.dirAxis[1] = Move::DOWN;
    me.dirAxis[2] = Move::LEFT;
    me.dirAxis[3] = Move::RIGHT;
}

void _shuffle_axes(SimpleUnbiasedAgent& me)
{
    // randomize axis order
    std::shuffle(me.agentAxis.begin(), me.agentAxis.end(), me.rng);
    std::shuffle(me.boardAxisX.begin(), me.boardAxisX.end(), me.rng);
    std::shuffle(me.boardAxisY.begin(), me.boardAxisY.end(), me.rng);
    std::shuffle(me.dirAxis.begin(), me.dirAxis.end(), me.rng);
}

SimpleUnbiasedAgent::SimpleUnbiasedAgent()
{
    std::random_device rd;  // non explicit seed
    rng = std::mt19937_64(rd());
    _init_axes(*this);
    reset();
}

SimpleUnbiasedAgent::SimpleUnbiasedAgent(long seed)
{
    rng = std::mt19937_64(seed);
    _init_axes(*this);
    reset();
}

void SimpleUnbiasedAgent::reset()
{
    // reset the internal state
    moveQueue.count = 0;
    recentPositions.count = 0;

    _shuffle_axes(*this);
}

// TODO: not modified
bool _UHasRPLoop(SimpleAgent& me)
{
    for(int i = 0; i < me.recentPositions.count / 2; i++)
    {
        if(!(me.recentPositions[i] == me.recentPositions[i + 2]))
        {
            return false;
        }
    }

    return true;
}

// not modified
bool _UCheckPos(const Board& b, int x, int y)
{
    return !util::IsOutOfBounds(x, y) && IS_WALKABLE(b.items[y][x]);
}

void _USafeDirections(const SimpleUnbiasedAgent& me, const Board& b, FixedQueue<Move, MOVE_COUNT>& q, int x, int y)
{
    for (Move m : me.dirAxis) 
    {
        Position desired = util::DesiredPosition(x, y, m);
        int d = IsInDanger(b, desired.x, desired.y);
        if(_UCheckPos(b, desired.x, desired.y) && _safe_condition(d))
        {
            q.AddElem(m);
        }
    }
}

Move _UMoveSafeOneSpace(SimpleUnbiasedAgent& me, const Board& b)
{
    const AgentInfo& a = b.agents[me.id];
    me.moveQueue.count = 0;
    
    std::shuffle(me.dirAxis.begin(), me.dirAxis.end(), me.rng);
    _USafeDirections(me, b, me.moveQueue, a.x, a.y);
    SortDirections(me.moveQueue, me.recentPositions, a.x, a.y);

    if(me.moveQueue.count == 0)
        return Move::IDLE;
    else
        return me.moveQueue[me.rng() % std::min(2, me.moveQueue.count)];
}

Move _UMoveTowardsSafePlace(const SimpleUnbiasedAgent &me, const Board& b, const RMap& r, int radius)
{
    int originX = r.source.x;
    int originY = r.source.y;

    for(int y : me.boardAxisY)
    {
        for(int x : me.boardAxisX)
        {
            if(y >= originY + radius || y < originY - radius)
                break;

            if(x >= originX + radius || x < originX - radius)
                break;

            if(util::IsOutOfBounds({x, y}) ||
                    std::abs(x - originX) + std::abs(y - originY) > radius) continue;

            if(r.GetDistance(x, y) != 0 && _safe_condition(IsInDanger(b, x, y)))
            {
                return MoveTowardsPosition(r, {x, y});
            }
        }
    }
    return Move::IDLE;
}

Move _UMoveTowardsEnemy(const SimpleUnbiasedAgent &me, const Board& b, const RMap& r, int radius)
{
    const AgentInfo& a = b.agents[me.id];

    for(int i : me.agentAxis)
    {
        const AgentInfo& other = b.agents[i];

        // ignore self, dead agents, invisible agents and teammates
        if(i == me.id || other.dead || !other.visible || !a.IsEnemy(other)) continue;

        if(std::abs(other.x - a.x) + std::abs(other.y - a.y) > radius)
        {
            continue;
        }
        // only move towards agents which we can actually reach
        else if(r.GetDistance(other.x, other.y) != 0)
        {
            return MoveTowardsPosition(r, {other.x, other.y});
        }

    }
    return Move::IDLE;
}

Move _UMoveTowardsPowerup(const SimpleUnbiasedAgent &me, const Board& b, const RMap& r, int radius)
{
    int minDist = std::numeric_limits<int>::max();
    const Position& a = r.source;
    Move moveTowardsPowerup = Move::IDLE;
    for(int y : me.boardAxisY)
    {
        for(int x : me.boardAxisX)
        {
            if(util::IsOutOfBounds(x, y) 
                || std::abs(x - a.x) > radius 
                || std::abs(y - a.y) > radius)
                continue;

            if(IS_POWERUP(b.items[y][x]))
            {
                int dist = r.GetDistance(x, y);
                if (dist != 0 && dist < minDist)
                {
                    Move m = MoveTowardsPosition(r, {x, y});
                    Position p = util::DesiredPosition(a.x, a.y, m);

                    if(!_safe_condition(IsInDanger(b, p.x, p.y)))
                    {
                        continue;
                    }
                    

                    minDist = dist;
                    moveTowardsPowerup = m;
                }
            }
        }
    }

    return moveTowardsPowerup;
}

bool _UIsEnemyInLine(const Board& b, int agentID, int distance)
{
    const AgentInfo& a = b.agents[agentID];

    for(int i = 0; i < bboard::AGENT_COUNT; i++)
    {
        // ignore self and dead agents
        if(i == agentID || b.agents[i].dead) continue;
        // ignore team
        if(a.team != 0 && a.team == b.agents[i].team) continue;

        // x line
        Position otherPos = b.agents[i].GetPos();

        if(otherPos.x == a.x && std::abs(otherPos.y - a.y) <= distance)
        {
            int min = std::min(otherPos.y, a.y);
            int diff = std::abs(otherPos.y - a.y);
            bool found = false;
            for(int y = min + 1; y < min + diff - 1; y++)
            {
                // no walls in the way (wood & powerups are destructible..)
                if(b.items[y][a.x] == Item::RIGID)
                {
                    found = true;
                    break;
                }
            }

            if(!found) return true;
        }
        else if(otherPos.y == a.y && std::abs(otherPos.x - a.x) <= distance)
        {
            int min = std::min(otherPos.x, a.x);
            int diff = std::abs(otherPos.x - a.x);
            bool found = false;
            for(int x = min + 1; x < min + diff - 1; x++)
            {
                // no walls in the way (wood & powerups are destructible..)
                if(b.items[a.y][x] == Item::RIGID)
                {
                    found = true;
                    break;
                }
            }

            if(!found) return true;
        }
    }
    return false;
}

Move SimpleUnbiasedAgent::decide(const Observation* obs)
{
    const Board& b = *obs;
    const AgentInfo& a = obs->agents[id];

    FillRMap(b, r, id);

    danger = IsInDanger(b, id);

    // the upper bound controls the greedyness of the agent (lower bound for more greed). 
    // Note that greedy agents seem to perform quite bad against non-greedy agents.
    if(danger > 0 && danger < 10)
    {
        Move m = _UMoveTowardsSafePlace(*this, b, r, danger);
        Position p = util::DesiredPosition(a.x, a.y, m);
        if(!util::IsOutOfBounds(p.x, p.y) && IS_WALKABLE(b.items[p.y][p.x]) &&
                _safe_condition(IsInDanger(b, p.x, p.y), 2))
        {
            return m;
        }
    }
    else if(a.bombCount < a.maxBombCount)
    {
        // prioritize enemy destruction
        // _UIsEnemyInLine(*state, me.id, (int)std::ceil(state->agents[me.id].bombStrength / 2.0f)))
        if(IsAdjacentEnemy(b, id, 1))
        {
            return Move::BOMB;
        }

        // target enemies across the map
        if(IsAdjacentEnemy(b, id, 14))
        {
            // if you're stuck in a loop try to break out by randomly selecting
            // an action ( we could IDLE but the mirroring of agents is tricky)
            if(_UHasRPLoop(*this)) {
                return Move(rng() % 4 + 1);
            }

            Move m = _UMoveTowardsEnemy(*this, b, r, 14);
            Position p = util::DesiredPosition(a.x, a.y, m);
            if(!util::IsOutOfBounds(p.x, p.y) && IS_WALKABLE(b.items[p.y][p.x]) &&
                    _safe_condition(IsInDanger(b, p.x, p.y), 3))
            {
                return m;
            }
        }

        if(IsAdjacentItem(b, id, 1, Item::WOOD))
        {
            return Move::BOMB;
        }
    }

    Move moveTowardsPowerup = _UMoveTowardsPowerup(*this, b, r, 5);
    if (moveTowardsPowerup != Move::IDLE)
    {
        return moveTowardsPowerup;
    }

    return _UMoveSafeOneSpace(*this, b);
}

}
