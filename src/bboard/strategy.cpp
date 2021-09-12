#include <limits>
#include <algorithm>
#include <unordered_set>

#include "bboard.hpp"
#include "colors.hpp"
#include "strategy.hpp"
#include "step_utility.hpp"

namespace bboard::strategy
{

////////////////////
// RMap Functions //
////////////////////

void RMap::SetDistance(int x, int y, int distance)
{
    map[y][x] = (map[y][x] & ~chalf) + distance;
}

void RMap::SetPredecessor(int x, int y, int xp, int yp)
{
    map[y][x] = (map[y][x] & chalf) + ((xp + BOARD_SIZE * yp) << 16);
}

int RMap::GetDistance(int x, int y) const
{
    return map[y][x] & chalf;
}

int RMap::GetPredecessor(int x, int y) const
{
    return map[y][x] >> 16;
}

template <typename T, int N>
inline RMapInfo TryAdd(const Board& b, FixedQueue<T, N>& q, RMap& r, Position& c, int cx, int cy)
{
    int dist = r.GetDistance(c.x, c.y);
    int item = b.items[cy][cx];
    if(!util::IsOutOfBounds(cx, cy) &&
            r.GetDistance(cx, cy) == 0 &&
            (IS_WALKABLE(item) || item >= Item::AGENT0))
    {
        r.SetPredecessor(cx, cy, c.x, c.y);
        r.SetDistance(cx, cy, dist + 1);

        // we compute paths to agent positions but don't
        // continue search
        if(item < Item::AGENT0)
            q.AddElem({cx, cy});
        return 0;
    }
    return 0;
}
// BFS
void FillRMap(const Board& b, RMap& r, int agentID)
{
    std::fill(r.map[0], r.map[0] + BOARD_SIZE * BOARD_SIZE, 0);
    
    const AgentInfo& a = b.agents[agentID];
    int x = a.x;
    int y = a.y;
    r.source = {x, y};

    FixedQueue<Position, BOARD_SIZE * BOARD_SIZE> queue;
    r.SetDistance(x, y, 0);
    queue.AddElem({x, y});

    RMapInfo result = 0;

    while(queue.count != 0)
    {

        Position& c = queue.PopElem();
        int dist = r.GetDistance(c.x, c.y);
        if(IsInBombRange(a.x, a.y, a.bombStrength, c) && dist < 10)
        {
            result |= 0b1;
        }

        if(c.x != x || c.y+1 != y)
            TryAdd(b, queue, r, c, c.x, c.y + 1);
        if(c.x != x || c.y-1 != y)
            TryAdd(b, queue, r, c, c.x, c.y - 1);
        if(c.x+1 != x || c.y != y)
            TryAdd(b, queue, r, c, c.x + 1, c.y);
        if(c.x-1 != x || c.y != y)
            TryAdd(b, queue, r, c, c.x - 1, c.y);

    }
    r.info = result;
}

///////////////////////
// General Functions //
///////////////////////

Move MoveTowardsPosition(const RMap& r, const Position& position)
{
    Position curr = position;
    for(int i = 0;; i++)
    {
        int idx = r.GetPredecessor(curr.x, curr.y);
        int y = idx / BOARD_SIZE;
        int x = idx % BOARD_SIZE;
        if(x == r.source.x && y == r.source.y)
        {
            if(curr.x > r.source.x) return Move::RIGHT;
            if(curr.x < r.source.x) return Move::LEFT;
            if(curr.y > r.source.y) return Move::DOWN;
            if(curr.y < r.source.y) return Move::UP;
        }
        else if(r.GetDistance(curr.x, curr.y) == 0)
        {
            return Move::IDLE;
        }
        curr = {x, y};
    }
}

Move MoveTowardsSafePlace(const Board& b, const RMap& r, int radius)
{
    int originX = r.source.x;
    int originY = r.source.y;
    // TODO: Should be y < originY + radius (?)
    for(int y = originY - radius; y < originY + radius; y++)
    {
        for(int x = originX - radius; x < originY + radius; x++)
        {
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

Move MoveTowardsPowerup(const Board& b, const RMap& r, int radius)
{
    const Position& a = r.source;
    for(int y = a.y - radius; y <= a.y + radius; y++)
    {
        for(int x = a.x - radius; x <= a.x + radius; x++)
        {
            if(util::IsOutOfBounds(x, y) ||
                    std::abs(x - a.x) + std::abs(y - a.y) > radius) continue;

            if(IS_POWERUP(b.items[y][x]))
            {
                return MoveTowardsPosition(r, {x, y});
            }
        }
    }

    return Move::IDLE;
}

Move MoveTowardsEnemy(const Board& b, const RMap& r, int agentID, int radius)
{
    const AgentInfo& a = b.agents[agentID];

    for(int i = 0; i < AGENT_COUNT; i++)
    {
        const AgentInfo& other = b.agents[i];

        // ignore self, dead agents and teammates
        if(i == agentID || other.dead || !a.IsEnemy(other)) continue;

        if(std::abs(other.x - a.x) + std::abs(other.y - a.y) > radius)
        {
            continue;
        }
        else
        {
            return MoveTowardsPosition(r, {other.x, other.y});
        }

    }
    return Move::IDLE;
}

bool _CheckPos(const Board& b, int x, int y)
{
    return !util::IsOutOfBounds(x, y) && IS_WALKABLE(b.items[y][x]);
}

bool _safe_condition(int danger, int min)
{
    return danger == 0 || danger >= min;
}
void SafeDirections(const Board& b, FixedQueue<Move, MOVE_COUNT>& q, int x, int y)
{
    int d = IsInDanger(b, x + 1, y);
    if(_CheckPos(b, x + 1, y) && _safe_condition(d))
    {
        q.AddElem(Move::RIGHT);
    }

    d = IsInDanger(b, x - 1, y);
    if(_CheckPos(b, x - 1, y) && _safe_condition(d))
    {
        q.AddElem(Move::LEFT);
    }

    d = IsInDanger(b, x, y + 1);
    if(_CheckPos(b, x, y + 1) && _safe_condition(d))
    {
        q.AddElem(Move::DOWN);
    }

    d = IsInDanger(b, x, y - 1);
    if(_CheckPos(b, x, y - 1) && _safe_condition(d))
    {
        q.AddElem(Move::UP);
    }
}

int IsInDanger(const Board& b, int agentID)
{
    const AgentInfo& a = b.agents[agentID];
    return IsInDanger(b, a.x, a.y);
}

int IsInDanger(const Board& b, int x, int y)
{
    int minTime = std::numeric_limits<int>::max();
    // TODO: add consideration for chained bomb explosions
    for(int i = 0; i < b.bombs.count; i++)
    {
        const Bomb& bomb = b.bombs[i];
        if(IsInBombRange(BMB_POS_X(bomb), BMB_POS_Y(bomb), BMB_STRENGTH(bomb), {x,y}))
        {
            if(BMB_TIME(bomb) < minTime)
            {
                minTime = BMB_TIME(bomb);
            }
        }
    }

    if(minTime == std::numeric_limits<int>::max())
        minTime = 0;

    return minTime;
}

void PrintMap(RMap &r)
{
    std::string res = "";
    for(int i = 0; i < BOARD_SIZE; i++)
    {
        for(int j = 0; j < BOARD_SIZE; j++)
        {
            res += (r.GetDistance(j, i) >= 10 ? "" : " ");
            res += std::to_string(r.GetDistance(j, i)) + " ";
        }
        res += "\n";
    }
    std::cout << res;
}


void PrintPath(RMap &r, Position from, Position to)
{
    std::array<Position, BOARD_SIZE * BOARD_SIZE> path;
    std::unordered_set<Position> pathx;

    path[0] = {to.x, to.y};
    Position curr = path[0];

    for(uint i = 0; !(curr == from); i++)
    {
        pathx.insert(curr);
        int idx = r.GetPredecessor(curr.x, curr.y);
        int y = idx / BOARD_SIZE;
        int x = idx % BOARD_SIZE;
        curr = path[i] = {x, y};
    }

    for(int i = 0; i < BOARD_SIZE; i++)
    {
        for(int j = 0; j < BOARD_SIZE; j++)
        {
            std::cout << (r.GetDistance(j, i) >= 10 ? "" : " ");
            auto dist = std::to_string(r.GetDistance(j, i));
            std::cout << (pathx.count({j, i}) ? FRED(dist) : dist) << " ";
        }
        std::cout << std::endl;
    }
}

bool IsAdjacentEnemy(const Board& b, int agentID, int distance)
{
    const AgentInfo& a = b.agents[agentID];

    for(int i = 0; i < bboard::AGENT_COUNT; i++)
    {
        const AgentInfo& other = b.agents[i];

        // ignore self, dead agents and teammates
        if(i == agentID || other.dead || !a.IsEnemy(other)) continue;

        // manhattan dist
        if((std::abs(other.x - a.x) + std::abs(other.y - a.y)) <= distance)
        {
            return true;
        }
    }
    return false;
}

bool IsAdjacentItem(const Board& b, int agentID, int distance, Item item)
{
    const AgentInfo& a = b.agents[agentID];
    const int originX = a.x;
    const int originY = a.y;
    for(int y = originY - distance; y <= originY + distance; y++)
    {
        for(int x = originX - distance; x <= originX + distance; x++)
        {
            if(util::IsOutOfBounds(x, y) ||
                    std::abs(x - originX) + std::abs(y - originY) > distance) continue;

            int currItem = b.items[y][x];
            if(IS_WOOD(item) && IS_WOOD(currItem))
            {
                return true;
            }
            if(currItem == item)
            {
                return true;
            }
        }
    }
    return false;
}

}
