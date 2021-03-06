#include "bboard.hpp"
#include "agents.hpp"
#include "strategy.hpp"
#include "step_utility.hpp"

using namespace bboard;
using namespace bboard::strategy;

namespace agents
{

SimpleAgent::SimpleAgent()
{
    std::random_device rd;  // non explicit seed
    rng = std::mt19937_64(rd());
}

SimpleAgent::SimpleAgent(long seed)
{
    rng = std::mt19937_64(seed);
}

void SimpleAgent::reset()
{
    // reset the internal state
    moveQueue.count = 0;
    recentPositions.count = 0;
}

bool _HasRPLoop(SimpleAgent& me)
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

Move _MoveSafeOneSpace(SimpleAgent& me, const Board& b)
{
    const AgentInfo& a = b.agents[me.id];
    me.moveQueue.count = 0;
    SafeDirections(b, me.moveQueue, a.x, a.y);
    SortDirections(me.moveQueue, me.recentPositions, a.x, a.y);

    if(me.moveQueue.count == 0)
        return Move::IDLE;
    else
        return me.moveQueue[me.rng() % std::min(2, me.moveQueue.count)];
}


Move SimpleAgent::decide(const Observation* obs)
{
    const Board& b = *obs;
    const AgentInfo& a = obs->agents[id];
    
    FillRMap(b, r, id);

    danger = IsInDanger(b, id);

    if(danger > 0) // ignore danger if not too high
    {
        Move m = MoveTowardsSafePlace(b, r, danger);
        Position p = util::DesiredPosition(a.x, a.y, m);
        if(!util::IsOutOfBounds(p.x, p.y) && IS_WALKABLE(b.items[p.y][p.x]) &&
                _safe_condition(IsInDanger(b, p.x, p.y), 2))
        {
            return m;
        }
    }
    else if(a.bombCount < a.maxBombCount)
    {
        //prioritize enemy destruction
        if(IsAdjacentEnemy(b, id, 1))
        {
            return Move::BOMB;
        }

        if(IsAdjacentEnemy(b, id, 7))
        {
            // if you're stuck in a loop try to break out by randomly selecting
            // an action ( we could IDLE but the mirroring of agents is tricky)
            if(_HasRPLoop(*this)) {
                return Move(rng() % 5);
            }

            Move m = MoveTowardsEnemy(b, r, id, 7);
            Position p = util::DesiredPosition(a.x, a.y, m);
            if(!util::IsOutOfBounds(p.x, p.y) && IS_WALKABLE(b.items[p.y][p.x]) &&
                    _safe_condition(IsInDanger(b, p.x, p.y), 5))
            {
                return m;
            }
        }

        if(IsAdjacentItem(b, id, 1, Item::WOOD))
        {
            return Move::BOMB;
        }
    }

    return _MoveSafeOneSpace(*this, b);
}

Move SimpleAgent::act(const Observation* obs)
{
    const AgentInfo& a = obs->agents[id];

    Move m = decide(obs);
    Position p = util::DesiredPosition(a.x, a.y, m);

    if(recentPositions.RemainingCapacity() == 0)
    {
        recentPositions.PopElem();
    }
    recentPositions.AddElem(p);

    return m;
}

void SimpleAgent::PrintDetailedInfo()
{
    for(int i = 0; i < recentPositions.count; i++)
    {
        std::cout << recentPositions[i] << std::endl;
    }
}

}

