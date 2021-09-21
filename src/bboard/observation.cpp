#include <thread>
#include <chrono>
#include <functional>
#include <algorithm>
#include <random>

#include <bits/stdc++.h>

#include "bboard.hpp"
#include "step_utility.hpp"

namespace bboard
{

inline void _copyAgentInfos(const Board& from, Board& to)
{
    for(uint i = 0; i < AGENT_COUNT; i++)
    {
        to.agents[i] = from.agents[i];
    }
}

inline void _filterFlames(const State& state, Observation& obs, Position pos, int viewRange)
{
    obs.currentFlameTime = -1;
    obs.flames.count = 0;

    assert(state.currentFlameTime != -1);

    int cumulativeTimeLeft = 0;
    for(int i = 0; i < state.flames.count; i++)
    {
        Flame f = state.flames[i];
        cumulativeTimeLeft += f.timeLeft;

        if(InViewRange(f.position, pos.x, pos.y, viewRange))
        {
            f.timeLeft = cumulativeTimeLeft;
            obs.flames.AddElem(f);
        }
    }

    obs.currentFlameTime = util::OptimizeFlameQueue(obs);
}

void Observation::Get(const State& state, const uint agentID, const ObservationParameters obsParams, Observation& observation)
{
    observation.agentID = agentID;
    observation.params = obsParams;
    observation.timeStep = state.timeStep;

    // fully observable environment
    if(obsParams.exposePowerUps && !obsParams.agentPartialMapView && obsParams.agentInfoVisibility == AgentInfoVisibility::All)
    {
        observation.CopyFrom(state);
        return;
    }

    // board observations

    if(obsParams.agentPartialMapView)
    {
        // hide invisible area
        AgentInfo info = state.agents[agentID];

        int leftFogCount = std::max(0, info.x - obsParams.agentViewSize);
        int rightFogBegin = std::min(BOARD_SIZE, info.x + obsParams.agentViewSize + 1);
        int rightFogCount = BOARD_SIZE - rightFogBegin;
        int viewRowLength = rightFogBegin - leftFogCount;

        for(int y = 0; y < BOARD_SIZE; y++)
        {
            if(std::abs(y - info.y) > obsParams.agentViewSize)
            {
                // fill the whole row with fog
                std::fill_n(&observation.items[y][0], BOARD_SIZE, bboard::Item::FOG);
            }
            else
            {
                // row is inside the agent's view, fill the row partially with fog

                // cells on the left
                std::fill_n(&observation.items[y][0], leftFogCount, bboard::Item::FOG);

                // copy board items
                if(obsParams.exposePowerUps)
                {
                    std::copy_n(&state.items[y][leftFogCount], viewRowLength, &observation.items[y][leftFogCount]);
                }
                else
                {
                    for(int x = leftFogCount; x < rightFogBegin; x++)
                    {
                        int item = state.items[y][x];
                        if(IS_WOOD(item))
                        {
                            // erase the powerup information
                            observation.items[y][x] = Item::WOOD;
                        }
                        else
                        {
                            observation.items[y][x] = item;
                        }
                    }
                }

                // cells on the right
                std::fill_n(&observation.items[y][rightFogBegin], rightFogCount, bboard::Item::FOG);
            }
        }

        // filter bomb objects
        observation.bombs.count = 0;
        for(int i = 0; i < state.bombs.count; i++)
        {
            int b = state.bombs[i];

            if(InViewRange(info.x, info.y, BMB_POS_X(b), BMB_POS_Y(b), obsParams.agentViewSize))
            {
                observation.bombs.AddElem(b);
            }
        }

        // and flames
        _filterFlames(state, observation, info.GetPos(), obsParams.agentViewSize);
    }
    else
    {
        // full view on the arena
        observation.CopyFrom(state, false);

        if(!obsParams.exposePowerUps)
        {
            for(int y = 0; y < BOARD_SIZE; y++)
            {
                for(int x = 0; x < BOARD_SIZE; x++)
                {
                    int item = observation.items[y][x];
                    if(IS_WOOD(item) && item != Item::WOOD)
                    {
                        // erase the powerup information
                        observation.items[y][x] = Item::WOOD;
                    }
                }
            }
        }
    }

    // agent observations
    const AgentInfo& self = state.agents[agentID];
    
    // always observe self
    observation.agents[agentID] = self;

    // add others if visible
    for(uint i = 0; i < AGENT_COUNT; i++)
    {
        if(i == agentID) continue;

        const AgentInfo& other = state.agents[i];
        AgentInfo& otherObservation = observation.agents[i];

        if(InViewRange(self.x, self.y, other.x, other.y, obsParams.agentViewSize))
        {
            // other agent is visible
            switch (obsParams.agentInfoVisibility)
            {
            case bboard::AgentInfoVisibility::OnlySelf:
                // add visibility information but no stats
                otherObservation.visible = true;
                otherObservation.x = other.x;
                otherObservation.y = other.y;
                otherObservation.statsVisible = false;
                break;
            case bboard::AgentInfoVisibility::InView:
            case bboard::AgentInfoVisibility::All:
                // also add complete stats
                otherObservation = other;
                break;
            }
        }
        else
        {
            // other agent is visible
            if (obsParams.agentInfoVisibility == bboard::AgentInfoVisibility::All)
            {
                // stats are visible, initialize default
                otherObservation = other;
            }
            else
            {
                otherObservation.statsVisible = false;
            }

            // ..but the agent itself is not visible!
            otherObservation.visible = false;
            // we don't know much about this agent and want to ignore it
            // use unique positions out of bounds to be compatible with the destination checks
            otherObservation.x = -i;
            otherObservation.y = -1;
        }

        // however, we always know whether this agent is alive and in which team it is
        otherObservation.dead = other.dead;
        otherObservation.team = other.team;
    }
}

void Observation::ToState(State& state) const
{
    // initialize the board of the state
    state.CopyFrom(*this, false);

    // optimize flame queue
    state.currentFlameTime = util::OptimizeFlameQueue(state);

    int aliveAgents = 0;
    for(int i = 0; i < AGENT_COUNT; i++)
    {
        AgentInfo obsInfo = agents[i];
        AgentInfo& stateInfo = state.agents[i];

        if(!obsInfo.statsVisible)
        {
            // copy the stats from the given state object
            obsInfo.bombCount = stateInfo.bombCount;
            obsInfo.maxBombCount = stateInfo.maxBombCount;
            obsInfo.bombStrength = stateInfo.bombStrength;
            obsInfo.canKick = stateInfo.canKick;

            obsInfo.statsVisible = true;
        }

        stateInfo = obsInfo;

        if(!stateInfo.dead)
        {
            aliveAgents += 1;
        }
    }
    state.aliveAgents = aliveAgents;

    util::CheckTerminalState(state);
}

void _addBomb(Board& board, Bomb bomb)
{
    for(int i = 0; i < board.bombs.count; i++)
    {
        // add it at the correct position (sorted list)
        if(BMB_TIME(bomb) > BMB_TIME(board.bombs[i]))
        {
            if(i == 0)
            {
                break;
            }
            else
            {
                board.bombs.AddElem(bomb, i - 1);
                return;
            }
        }
    }

    // add it at the end
    board.bombs.AddElem(bomb);
}

void _addBombsFromObservation(State& state, const Observation& obs)
{
    if(!obs.params.agentPartialMapView)
    {
        state.bombs = obs.bombs;
        return;
    }

    Position center = obs.agents[obs.agentID].GetPos();
    std::unordered_set<Position> positions(obs.bombs.count);

    // remember the positions of all currently existing bombs
    for(int i = 0; i < obs.bombs.count; i++)
    {
        positions.insert(BMB_POS(obs.bombs[i]));
    }

    auto oldBombs = state.bombs;

    // set known bombs
    state.bombs = obs.bombs;

    // reinsert old bombs
    for(int i = 0; i < oldBombs.count; i++)
    {
        Bomb b = oldBombs[i];
        Position bPos = BMB_POS(b);

        if(InViewRange(center, bPos, obs.params.agentViewSize))
        {
            // bombs in view range are already handled correctly
            continue;
        }

        auto res = positions.find(bPos);
        if(res != positions.end())
        {
            // there is already a bomb at this position
            continue;
        }

        // otherwise: reduce timer and add the bomb at the right index (sorted list)
        ReduceBombTimer(b);
        // TODO: Handle bomb movement (?)
        SetBombDirection(b, Direction::IDLE);

        _addBomb(state, b);
    }
}

void _convertToAbsoluteFlameTimes(Board& board)
{
    if (board.currentFlameTime == -1) {
        return;
    }

    board.currentFlameTime = -1;
    int cumulativeFlameTime = 0;
    for (int i = 0; i < board.flames.count; i++) {
        cumulativeFlameTime += board.flames[i].timeLeft;
        board.flames[i].timeLeft = cumulativeFlameTime;
    }
}

void _addFlamesFromObservation(State& state, const Observation& obs)
{
    if(!obs.params.agentPartialMapView)
    {
        state.flames = obs.flames;
        state.currentFlameTime = obs.currentFlameTime;
        return;
    }

    Position center = obs.agents[obs.agentID].GetPos();
    std::unordered_set<Position> knownFlamePositions(state.flames.count);

    // remember the positions of all old flames
    for(int i = 0; i < state.flames.count; i++)
    {
        knownFlamePositions.insert(state.flames[i].position);
    }

    // temporarily convert flame times
    _convertToAbsoluteFlameTimes(state);

    // insert new flames
    int cumulativeFlameTime = 0;
    for(int i = 0; i < obs.flames.count; i++)
    {
        Flame f = obs.flames[i];
        cumulativeFlameTime += f.timeLeft;

        // only add new flames with absolute time
        if(knownFlamePositions.find(f.position) != knownFlamePositions.end())
        {
            f.timeLeft = cumulativeFlameTime;
            state.flames.AddElem(f);
        }
    }

    state.currentFlameTime = util::OptimizeFlameQueue(state);
}

void Observation::VirtualStep(State& state, bool keepAgents, bool keepBombs, int (*itemAge)[BOARD_SIZE][BOARD_SIZE]) const
{
    if (state.timeStep != timeStep - 1)
    {
        std::cerr << "WARNING: Updating state with observation that is not its next time step!" 
                  << " State timestep: " << state.timeStep << ", obs timestep: " << timeStep << std::endl;
    }

    state.timeStep = timeStep;

    // start by merging the agent information
    for (int i = 0; i < AGENT_COUNT; i++)
    {
        const AgentInfo& obsAgent = agents[i];
        AgentInfo& stateAgent = state.agents[i];

        // update main information
        stateAgent.dead = obsAgent.dead;
        stateAgent.team = obsAgent.team;

        // update position and stats if available
        if (obsAgent.visible)
        {
            stateAgent.visible = true;
            stateAgent.x = obsAgent.y;
            stateAgent.y = obsAgent.y;
        }
        else if (!keepAgents)
        {
            stateAgent.visible = false;
        }

        if (obsAgent.statsVisible)
        {
            stateAgent.statsVisible = true;
            stateAgent.bombCount = obsAgent.bombCount;
            stateAgent.bombStrength = obsAgent.bombStrength;
            stateAgent.maxBombCount = obsAgent.maxBombCount;
            stateAgent.canKick = obsAgent.canKick;
        }
        else if (!keepAgents)
        {
            stateAgent.statsVisible = false;
        }
    }

    // add new information to the board

    for(int y = 0; y < BOARD_SIZE; y++)
    {
        for(int x = 0; x < BOARD_SIZE; x++)
        {
            int item = items[y][x];

            if(item != Item::FOG)
            {
                // new item is not fog => directly update the state
                state.items[y][x] = item;
                if(itemAge != nullptr)
                {
                    (*itemAge)[y][x] = 0;
                }
            }
            else
            {
                // try to reconstruct fog using the last state
                int oldItem = state.items[y][x];

                if(oldItem == Item::FOG)
                {
                    // nothing to change here
                    continue;
                }

                // no fog here -> reconstruct

                if(oldItem >= Item::AGENT0)
                {
                    const int id = oldItem - Item::AGENT0;
                    if(!keepAgents || agents[id].visible)
                    {
                        // skip agents when we ignore them or when they
                        // are already visible in our current observation
                        // (-> they moved)
                        oldItem = Item::PASSAGE;
                    }
                }

                if(oldItem == Item::BOMB)
                {
                    if(keepBombs)
                    {
                        // add bombs (may explode later)
                        oldItem = Item::BOMB;
                    }
                    else
                    {
                        // treat the bomb as a passage
                        oldItem = Item::PASSAGE;
                    }
                }

                state.items[y][x] = oldItem;

                if(itemAge != nullptr)
                {
                    // increase the age of that reconstructed item
                    (*itemAge)[y][x] += 1;
                }
            }
        }
    }

    // tick flames in the state (they are 1 step older now)
    util::TickFlames(&state);

    if(keepBombs)
    {
        _addBombsFromObservation(state, *this);
        // after adding old bombs, let them explode (if necessary)
        util::ExplodeBombs(&state);
    }
    else
    {
        state.bombs = bombs;
    }

    _addFlamesFromObservation(state, *this);
    util::CheckTerminalState(state);
}

void Observation::Print(bool clearConsole) const
{
    Board::Print(clearConsole);
}

}
