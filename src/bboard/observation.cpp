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

inline bool _agentVisibleInObservation(int x1, int y1, const AgentInfo& agent, const ObservationParameters& obsParams)
{
    return agent.visible && (!obsParams.agentPartialMapView || InViewRange(x1, y1, agent.x, agent.y, obsParams.agentViewSize));
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
                        // erase the powerup information from wood and flames
                        if(IS_WOOD(item))
                        {
                            observation.items[y][x] = Item::WOOD;
                        }
                        else if (IS_FLAME(item))
                        {
                            observation.items[y][x] = CLEAR_POWFLAG(item);
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

        if(_agentVisibleInObservation(self.x, self.y, other, obsParams))
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
            // other agent is not visible in state or not in view range

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

    const AgentInfo& self = agents[agentID];

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
            stateAgent.x = obsAgent.x;
            stateAgent.y = obsAgent.y;
        }
        else if (
            // reset agents when we don't see them.. and don't want to keep them
            !keepAgents 
            // or when they should have been in our current observation
            || !params.agentPartialMapView 
            || InViewRange(self.GetPos(), stateAgent.GetPos(), params.agentViewSize)
        )
        {
            stateAgent.visible = false;
            // update invalid position to be compatible with destination checks 
            stateAgent.x = -i;
            stateAgent.y = -1;
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

/**
 * @brief Checks if there was a bomb at checkPosition in the (old) board or if an agent in newObs has kicked this bomb.
 * 
 * @param board the (old) board
 * @param newObs the (new) observation, one step after the board
 * @param checkPosition the position the bomb should have passed
 * @param checkDirection the direction the bomb took after being at checkPosition
 * @param checkTime the time of the bomb that we are looking for
 * @param checkRange the range of the bomb that we are looking for
 * @param recursionDepth temporary variable to avoid infinite loops
 * @return int the old index of the bomb (on the board, not the observation) or -1 if the bomb could not be found
 */
int _backtrack_bomb_id(const bboard::Board* board, const bboard::Observation* newObs, bboard::Position checkPosition, bboard::Direction checkDirection, int checkTime, int checkRange, int recursionDepth=0)
{
    // avoid infinite loops
    if (recursionDepth >= AGENT_COUNT || bboard::util::IsOutOfBounds(checkPosition)) {
        return -1;
    }

    // std::cout << "Checking " << checkPosition << std::endl;
    int stateItem = board->items[checkPosition.y][checkPosition.x];
    if (stateItem == bboard::Item::BOMB)
    {
        // is this the bomb we are looking for?
        int candidateBombIndex = board->GetBombIndex(checkPosition.x, checkPosition.y);
        const bboard::Bomb& candidateBomb = board->bombs[candidateBombIndex];
        // std::cout << "Checking bomb at state position " << checkPosition << std::endl;
        if (bboard::BMB_STRENGTH(candidateBomb) == checkRange && bboard::BMB_TIME(candidateBomb) == checkTime && bboard::BMB_DIR(candidateBomb) == (int)checkDirection) {
            return candidateBombIndex;
        }
        else {
            return -1;
        }
    }

    int obsItem = newObs->items[checkPosition.y][checkPosition.x];
    if (bboard::IS_AGENT(obsItem))
    {
        // std::cout << "Found agent in new observation at " << checkPosition << std::endl;
        // this agent could have kicked the bomb
        // example:
        // step 0: 0   b (bomb moves left, agent moves right => kick)
        // step 1:   0 b (bomb moves right)
        // we want to find the original position of the bomb in step 0, which is not `current position - movement`
        // note that there could be chains, i.e. a bomb can be kicked multiple times in one step by different agents

        // first check if the agent moved in the correct direction
        bboard::Position oldCandidatePosition = bboard::util::OriginPosition(checkPosition.x, checkPosition.y, bboard::Move(checkDirection));
        // std::cout << "Checking old candidate position " << oldCandidatePosition << std::endl;
        int oldCandidateStateItem = board->items[oldCandidatePosition.y][oldCandidatePosition.x];
        if (oldCandidateStateItem == obsItem) {
            // std::cout << "Agent was at old position!" << std::endl;
            // this is the same agent, hence it has kicked the bomb that arrived at this position (checkPosition) from any direction => search for it!
            for (bboard::Direction newCheckDirection : {bboard::Direction::UP, bboard::Direction::DOWN, bboard::Direction::LEFT, bboard::Direction::RIGHT}) {
                if (newCheckDirection == checkDirection) {
                    // this is where the agent came from, we don't have to check this.
                    continue;
                }

                // move back one field and look for the bomb
                bboard::Position newCheckPosition = bboard::util::OriginPosition(checkPosition.x, checkPosition.y, bboard::Move(newCheckDirection));
                int res = _backtrack_bomb_id(board, newObs, newCheckPosition, newCheckDirection, checkTime, checkRange, recursionDepth + 1);
                if (res != -1) {
                    return res;
                }
            }
        }
    }

    return -1;
}

int trackStatsErrorCounter = 3;
/**
 * @brief Tries to find a bomb from newObs in the given board.
 * 
 * @param board the (old) board
 * @param newObs the (new) observation, one step after the board
 * @return int the old index of the bomb (on the board, not the observation) or -1 if the bomb could not be found
 */
int _backtrack_bomb_id(const bboard::Board* board, const bboard::Observation* newObs, Bomb b)
{
    bboard::Position bombPos = bboard::BMB_POS(b);
    int bombMovement = bboard::BMB_DIR(b);

    // check if there is a bomb at the previous position according to the bomb's movement
    bboard::Position bombOrigin = bboard::util::OriginPosition(bombPos.x, bombPos.y, bboard::Move(bombMovement));
    int oldOriginBombIndex = board->GetBombIndex(bombOrigin.x, bombOrigin.y);
    
    if (oldOriginBombIndex != -1) {
        // simple case: we found the bomb (e.g. no or simple movement)
        return oldOriginBombIndex;
    }

    // the bomb is not where it's supposed to be => it was kicked while moving. We have to backtrack it
    int bombId = _backtrack_bomb_id(board, newObs, bombOrigin, bboard::Direction(bombMovement), bboard::BMB_TIME(b) + 1, bboard::BMB_STRENGTH(b));

    // print warning when backtracking failed (e.g. partial observability)
    if (bombId == -1 && trackStatsErrorCounter > 0) {
        trackStatsErrorCounter--;
        std::cout << "Warning: could not find owner of bomb at " << bombPos << " (previous position according to movement: " << bombOrigin 
                << ") with the TrackStats heuristic. Maybe the board is not fully visible? This message will only be repeated " << trackStatsErrorCounter << " more times." << std::endl;
        std::cout << "Previous board:" << std::endl;
        board->Print();
        std::cout << "Previous bomb positions: ";
        for (int k = 0; k < board->bombs.count; k++) {
            std::cout << bboard::BMB_POS(board->bombs[k]) << " ";
        }
        std::cout << std::endl;
        std::cout << "Observation:" << std::endl;
        newObs->Print();
    }

    return bombId;
}

void _count_bomb_if_stats_invisible(AgentInfo& info)
{
    // only count if stats are not visible
    if (!info.statsVisible)
    {
        info.bombCount++;
        // correct max bomb count when we missed some powerup collection
        if (info.bombCount > info.maxBombCount)
        {
            info.maxBombCount = info.bombCount;
        }
    }
}

bool _has_kicked_bomb(const bboard::Board* board, const bboard::Observation* newObs, int agentID)
{
    const bboard::AgentInfo& info = newObs->agents[agentID];
    const bboard::AgentInfo& oldInfo = board->agents[agentID];

    // ignore agents that are not on the board
    if (info.dead || oldInfo.dead || !info.visible || !oldInfo.visible) {
        return false;
    }

    // agents that don't move cannot kick bombs
    if (info.GetPos() == oldInfo.GetPos()) {
        return false;
    }

    Position movement = info.GetPos() - oldInfo.GetPos();
    Position potentialKickPosition = info.GetPos() + movement;

    // agent cannot kick when the bomb would now be outside of the board
    if (bboard::util::IsOutOfBounds(potentialKickPosition)) {
        return false;
    }
    
    // agent cannot kick when there is no bomb
    int potentialKickItem = newObs->items[potentialKickPosition.y][potentialKickPosition.x];
    if (potentialKickItem != bboard::Item::BOMB) {
        return false;
    }

    // backtrack bomb id (return false only upon backtracking errors)
    bboard::Bomb* bomb = newObs->GetBomb(potentialKickPosition.x, potentialKickPosition.y);
    if (!bomb) {
        return false;
    }
    int oldBombId = _backtrack_bomb_id(board, newObs, *bomb);
    if (oldBombId == -1) {
        return false;
    }

    // the bomb did not change its direction, agent did not kick
    bboard::Bomb oldBomb = board->bombs[oldBombId];
    if (BMB_DIR(*bomb) == BMB_DIR(oldBomb)) {
        return false;
    }

    // there is a bomb and it changed its direction => has been kicked by the agent
    return true;
}

void Observation::TrackStats(const Board& oldBoard)
{
    bool allStatsAreVisible = true;
    for (int i = 0; i < bboard::AGENT_COUNT; i++) {
        bboard::AgentInfo info = agents[i];
        allStatsAreVisible = allStatsAreVisible && info.statsVisible;
    }

    if (timeStep == 0 || allStatsAreVisible) {
        // we already know the stats and don't have to do anything
        return;
    }

    for (int i = 0; i < bboard::AGENT_COUNT; i++) {
        bboard::AgentInfo& info = agents[i];
        const bboard::AgentInfo& oldInfo = oldBoard.agents[i];

        if (info.dead)
        {
            info.maxBombCount = oldInfo.maxBombCount;
            info.bombStrength = oldInfo.bombStrength;
            info.canKick = oldInfo.canKick;
            info.bombCount = 0;

            // we have to skip dead agents here, they cannot be new owners
            continue;
        }

        // track unknown stats of agents
        if (!info.statsVisible) {
            // we will count active bombs later
            info.bombCount = 0;
            info.maxBombCount = oldInfo.maxBombCount;
            info.bombStrength = oldInfo.bombStrength;
            info.canKick = oldInfo.canKick;

            // we can track stats of visible agents to some extent
            if (info.visible) {
                // track collected power ups (if powerup is visible)
                int oldItem = oldBoard.items[info.y][info.x];
                switch (oldItem)
                {
                case bboard::Item::EXTRABOMB:
                    info.maxBombCount = oldInfo.maxBombCount + 1;
                    break;

                case bboard::Item::INCRRANGE:
                    info.bombStrength = oldInfo.bombStrength + 1;
                    break;

                case bboard::Item::KICK:
                    info.canKick = true;
                    break;
                
                default:
                    break;
                }

                // if we see the agent kicking, then we missed a kick powerup item
                if (!info.canKick && _has_kicked_bomb(&oldBoard, this, i))
                {
                    info.canKick = true;
                } 
            }
        }

        // if the agent stands above a bomb
        // 1) we directly know the max strength
        // 2) we know the owner of the bomb
        int bombIndex = GetBombIndex(info.x, info.y);
        if (bombIndex != -1) {
            bboard::Bomb& b = bombs[bombIndex]; 
            // set agent strength
            info.bombStrength = bboard::BMB_STRENGTH(b);
            // set agent id
            bboard::SetBombID(b, i);
        }
    }

    // count bombs and reconstruct bomb ids of moving bombs
    for (int i = 0; i < bombs.count; i++) {
        bboard::Bomb& b = bombs[i];
        int bombOwner = bboard::BMB_ID(b);
        if (bombOwner >= 0 && bombOwner < bboard::AGENT_COUNT) {
            // bomb owner is known => count bombs
            _count_bomb_if_stats_invisible(agents[bombOwner]);
        }
        else {
            // bomb owner is not known yet, try to find it in the old board
            int oldBombId = _backtrack_bomb_id(&oldBoard, this, b);
            if (oldBombId != -1) {
                bombOwner = BMB_ID(oldBoard.bombs[oldBombId]);
            }
            bool foundAgent = bombOwner >= 0 && bombOwner < bboard::AGENT_COUNT;
            if (foundAgent) {
                bboard::SetBombID(b, bombOwner);
                _count_bomb_if_stats_invisible(agents[bombOwner]);
            }
        }
    }

    for (int i = 0; i < bboard::AGENT_COUNT; i++) {
        // we act as if all agent stats are visible now
        agents[i].statsVisible = true;
    }
}

void Observation::Print(bool clearConsole) const
{
    Board::Print(clearConsole);
}

}
