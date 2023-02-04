#include <iostream>

#include "catch.hpp"
#include "bboard.hpp"

using namespace bboard;

void REQUIRE_CORRECT_FOG(const State& s, uint agentID, const Board& b, int viewRange)
{
    bool foundIncorrectCell = false;
    for(int y = 0; y < BOARD_SIZE; y++)
    {
        for(int x = 0; x < BOARD_SIZE; x++)
        {
            bool isFog = b.items[y][x] == Item::FOG;
            bool isInViewRange = InViewRange(s.agents[agentID].GetPos(), x, y, viewRange);

            if(isFog == isInViewRange)
            {
                foundIncorrectCell = true;
                break;
            }
        }

        if(foundIncorrectCell)
            break;
    }
    REQUIRE(foundIncorrectCell == false);
}

TEST_CASE("View Range", "[observation]")
{
    // initialize some state
    State s;
    s.Init(GameMode::FreeForAll, 4, true);

    SECTION("No fog in state")
    {
        bool foundFog = false;
        for(int y = 0; y < BOARD_SIZE; y++)
        {
            for(int x = 0; x < BOARD_SIZE; x++)
            {
                if(s.items[y][x] == Item::FOG)
                {
                    foundFog = true;
                    break;
                }
            }

            if(foundFog)
                break;
        }
        REQUIRE(foundFog == false);
    }

    SECTION("Fog in observation 0")
    {
        ObservationParameters params;
        params.agentPartialMapView = true;
        params.agentViewSize = 3;

        Observation obs;
        Observation::Get(s, 0, params, obs);

        REQUIRE_CORRECT_FOG(s, 0, obs, params.agentViewSize);
    }

    SECTION("Fog in observation 2")
    {
        ObservationParameters params;
        params.agentPartialMapView = true;
        params.agentViewSize = 5;

        Observation obs;
        Observation::Get(s, 2, params, obs);

        REQUIRE_CORRECT_FOG(s, 2, obs, params.agentViewSize);
    }
}

TEST_CASE("Round trip", "[observation]")
{
    // initialize state
    State s;
    s.Init(GameMode::FreeForAll, 1234, true);

    // restrict view
    ObservationParameters params;
    params.agentPartialMapView = true;
    params.agentViewSize = 4;

    // get observation
    Observation obs;
    Observation::Get(s, 2, params, obs);

    // check fog
    REQUIRE_CORRECT_FOG(s, 2, obs, params.agentViewSize);

    // convert to state
    State s2;
    obs.ToState(s2);

    // fog is still there
    REQUIRE_CORRECT_FOG(s, 2, s2, params.agentViewSize);
    REQUIRE(s.aliveAgents == s2.aliveAgents);
    REQUIRE(s.agents[2].x == s2.agents[2].x);
    REQUIRE(s.agents[2].y == s2.agents[2].y);
    REQUIRE(s.agents[2].team == s2.agents[2].team);
    REQUIRE(s2.agents[2].visible == true);
}

TEST_CASE("Planning Step", "[observation]")
{
    ObservationParameters params;
    params.agentPartialMapView = true;
    params.agentViewSize = 4;

    State s, s2;
    Observation obs;

    s.Init(GameMode::FreeForAll, 1234, true);
    Observation::Get(s, 0, params, obs);
    obs.ToState(s2);

    Move m[4];
    m[0] = m[1] = m[2] = m[3] = Move::IDLE;

    AgentInfo& ownAgent = s2.agents[0];

    SECTION("Step")
    {
        // just check whether we can do a step
        s2.Step(m);
    }
    SECTION("Bombs explode")
    {
        REQUIRE(!ownAgent.dead);
        m[0] = Move::BOMB;
        s2.Step(m);
        m[0] = Move::IDLE;

        for(int _ = 0; _ < bboard::BOMB_LIFETIME; _++)
        {
            s2.Step(m);
        }

        REQUIRE(ownAgent.dead);
    }
}

void REQUIRE_ITEM_IF(int item, bool condition, int trueItem, int falseItem)
{
    if(condition)
    {
        REQUIRE(item == trueItem);
    }
    else
    {
        REQUIRE(item == falseItem);
    }
}

void REQUIRE_FLAME_IF(int item, bool condition, int falseItem)
{
    if(condition)
    {
        REQUIRE(bboard::IS_FLAME(item) == true);
    }
    else
    {
        REQUIRE(item == falseItem);
    }
}

void _print_flames(const Board& b) 
{
    for (int i = 0; i < b.flames.count - 1; i++) {
        std::cout << b.flames[i].timeLeft << ", ";
    }

    if (b.flames.count > 0) {
        std::cout << b.flames[b.flames.count - 1].timeLeft << std::endl;
    }
}

void _print_step(const State& s, const Observation& o)
{
    std::cout << "Step " << s.timeStep << " > State" << std::endl;
    s.Print();
    _print_flames(s);

    std::cout << "Step " << s.timeStep << " > Observation" << std::endl;
    o.Print();
    _print_flames(o);
}

void _print_step(const State& s, const State& r)
{
    std::cout << "Step " << s.timeStep << " > State" << std::endl;
    s.Print();
    _print_flames(s);

    std::cout << "Step " << r.timeStep << " > Reconstructed" << std::endl;
    r.Print();
    _print_flames(r);
}

TEST_CASE("Hidden Flames", "[observation]")
{
    bool print = false;

    ObservationParameters params;
    params.agentPartialMapView = true;
    params.agentViewSize = 1;

    State s;
    s.Clear(Item::PASSAGE);
    s.timeStep = 0;

    // 0 1
    // 3 2
    s.PutAgentsInCorners(0, 1, 2, 3, 1);

    Move m[AGENT_COUNT];
    std::fill_n(m, AGENT_COUNT, Move::IDLE);
    m[0] = Move::BOMB;

    // place a bomb
    s.Step(m);

    // move out of range
    m[0] = Move::RIGHT;
    s.Step(m);
    s.Step(m);

    // wait until the bomb explodes
    m[0] = Move::IDLE;
    for (int i = 0; i < bboard::BOMB_LIFETIME - 2; i++)
    {
        s.Step(m);
    }

    REQUIRE(bboard::IS_FLAME(s.items[1][1]));
    REQUIRE(bboard::IS_FLAME(s.items[1][2]));

    if (print) {
        s.Print();
        _print_flames(s);
    }

    Observation obs;
    Observation::Get(s, 0, params, obs);

    if (print) {
        obs.Print();
        _print_flames(obs);
    }

    // there is only one flame visible
    REQUIRE(obs.items[1][1] == Item::FOG);
    REQUIRE(bboard::IS_FLAME(obs.items[1][2]));

    // and the timeLeft of this flame is correct
    REQUIRE(obs.flames[0].timeLeft == bboard::FLAME_LIFETIME);
}

TEST_CASE("Bomb moves into fog", "[observation]")
{
    bool print = false;

    ObservationParameters params;
    params.agentPartialMapView = true;
    params.agentViewSize = 1;

    State trueState;
    trueState.Clear(Item::PASSAGE);
    trueState.timeStep = 0;

    trueState.PutAgentsInCorners(0, 1, 2, 3, 1);
    Move m[AGENT_COUNT];
    std::fill_n(m, AGENT_COUNT, Move::IDLE);

    // get planning state s
    Observation obs;
    Observation::Get(trueState, 0, params, obs);
    State s;
    obs.ToState(s);

    // place a bomb that moves to the right
    s.PutBomb(0, 0, 0, 2, 4, true);
    Bomb& b = *s.GetBomb(0, 0);
    SetBombDirection(b, bboard::Direction::RIGHT);

    if (print) {
        s.Print();
    }
    REQUIRE(s.items[0][0] == Item::BOMB);
    REQUIRE(s.bombs.count == 1);
    s.Step(m);
    if (print) {
        s.Print();
    }
    REQUIRE(s.items[0][1] == Item::BOMB);
    REQUIRE(s.bombs.count == 1);
    s.Step(m);
    if (print) {
        s.Print();
    }
    REQUIRE(s.items[0][2] == Item::BOMB);
    REQUIRE(s.bombs.count == 1);
    s.Step(m);
    if (print) {
        s.Print();
    }
    REQUIRE(s.items[0][2] == Item::PASSAGE);
    REQUIRE(s.bombs.count == 0);
}

TEST_CASE("Merge Observations", "[observation]")
{
    bool print = false;

    ObservationParameters params;
    params.agentPartialMapView = true;
    params.agentViewSize = 1;

    State s;
    s.Clear(Item::PASSAGE);
    s.timeStep = 0;

    // 0 1
    // 3 2
    s.PutAgentsInCorners(0, 1, 2, 3, 1);
    s.PutItem(0, 0, Item::RIGID);
    s.PutItem(0, 1, Item::WOOD);

    Observation obs;
    Observation::Get(s, 0, params, obs);

    // if (print) _print_step(s, obs);

    // all the items are within our view range
    REQUIRE(obs.items[0][0] == Item::RIGID);
    REQUIRE(obs.items[1][0] == Item::WOOD);
    REQUIRE(obs.items[2][0] == Item::PASSAGE);

    Move m[AGENT_COUNT];
    std::fill_n(m, AGENT_COUNT, Move::IDLE);
    m[0] = Move::RIGHT;

    int itemAge[BOARD_SIZE][BOARD_SIZE];
    std::fill_n(&itemAge[0][0], BOARD_SIZE * BOARD_SIZE, 0);

    State reconstructed;
    obs.ToState(reconstructed);

    // walk to the right until we see agent 1
    for(int i = 0; i < BOARD_SIZE - 4; i++)
    {
        s.Step(m);

        // the items are out of range now
        Observation::Get(s, 0, params, obs);
        REQUIRE(obs.items[0][0] == Item::FOG);
        REQUIRE(obs.items[1][0] == Item::FOG);
        REQUIRE(obs.items[2][0] == Item::FOG);

        // but can be reconstructed
        obs.VirtualStep(reconstructed, false, false, &itemAge);
        REQUIRE(reconstructed.items[0][0] == Item::RIGID);
        REQUIRE(reconstructed.items[1][0] == Item::WOOD);
        REQUIRE(reconstructed.items[2][0] == Item::PASSAGE);
        REQUIRE(itemAge[0][0] == (i + 1));
        REQUIRE(itemAge[1][1 + i] == 0);

        // if (print) _print_step(s, reconstructed);
    }

    // place a bomb next to agent 1
    REQUIRE(s.items[1][BOARD_SIZE - 3] == Item::AGENT0);
    REQUIRE(s.items[1][BOARD_SIZE - 2] == Item::AGENT1);
    m[0] = Move::BOMB;

    s.Step(m);
    REQUIRE(s.HasBomb(BOARD_SIZE - 3, 1));

    Observation::Get(s, 0, params, obs);
    obs.VirtualStep(reconstructed, false, false, &itemAge);

    // if (print) _print_step(s, reconstructed);

    // run away downwards
    m[0] = Move::DOWN;

    // bomb and target agent are directly visible on the first step
    s.Step(m);

    Observation::Get(s, 0, params, obs);
    REQUIRE(obs.items[1][BOARD_SIZE - 3] == Item::BOMB);
    REQUIRE(obs.items[1][BOARD_SIZE - 2] == Item::AGENT1);

    obs.VirtualStep(reconstructed, false, false, &itemAge);

    if (print) _print_step(s, reconstructed);

    for (bool trackBombs : {false, true})
    {
        for(bool trackAgents : {false, true})
        {
            SECTION("Track Bombs (" + std::to_string(trackBombs) + ") and Agents (" + std::to_string(trackAgents) + ")")
            {
                // ... but not in the following steps
                for(int i = 0; i < BOMB_LIFETIME - 2; i++)
                {
                    s.Step(m);

                    Observation::Get(s, 0, params, obs);
                    REQUIRE(obs.items[1][BOARD_SIZE - 3] == Item::FOG);
                    REQUIRE(obs.items[1][BOARD_SIZE - 2] == Item::FOG);

                    // however, they can be reconstructed
                    obs.VirtualStep(reconstructed, trackAgents, trackBombs, &itemAge);
                    if (print) _print_step(s, reconstructed);

                    REQUIRE_ITEM_IF(reconstructed.items[1][BOARD_SIZE - 3], trackBombs, Item::BOMB, Item::PASSAGE);
                    REQUIRE_ITEM_IF(reconstructed.items[1][BOARD_SIZE - 2], trackAgents, Item::AGENT1, Item::PASSAGE);
                }

                // reconstructed bombs eventually explode, even out of view
                s.Step(m);
                REQUIRE(bboard::IS_FLAME(s.items[1][BOARD_SIZE - 3]) == true);
                REQUIRE(s.agents[1].dead == true);

                Observation::Get(s, 0, params, obs);
                REQUIRE(obs.items[1][BOARD_SIZE - 3] == Item::FOG);

                obs.VirtualStep(reconstructed, trackAgents, trackBombs, &itemAge);
                if (print) _print_step(s, reconstructed);

                // center of the bomb
                REQUIRE_FLAME_IF(reconstructed.items[1][BOARD_SIZE - 3], trackBombs, Item::PASSAGE);
                // other flame items
                REQUIRE_FLAME_IF(reconstructed.items[2][BOARD_SIZE - 3], trackBombs, Item::PASSAGE);
                REQUIRE_FLAME_IF(reconstructed.items[0][BOARD_SIZE - 3], trackBombs, Item::PASSAGE);
                REQUIRE_FLAME_IF(reconstructed.items[1][BOARD_SIZE - 2], trackBombs, trackAgents ? Item::AGENT1 : Item::PASSAGE);
                REQUIRE_FLAME_IF(reconstructed.items[1][BOARD_SIZE - 4], trackBombs, Item::PASSAGE);
                REQUIRE(reconstructed.agents[1].dead == true);
                REQUIRE(itemAge[1][BOARD_SIZE - 3] == BOMB_LIFETIME - 1);

                if(trackBombs)
                {
                    // ... and the spawned flames eventually vanish
                    for(int i = 0; i < FLAME_LIFETIME - 1; i++)
                    {
                        s.Step(m);
                        Observation::Get(s, 0, params, obs);
                        obs.VirtualStep(reconstructed, true, true, &itemAge);

                        if (print) _print_step(s, reconstructed);
                    }

                    s.Step(m);
                    REQUIRE(s.items[1][BOARD_SIZE - 3] == Item::PASSAGE);

                    Observation::Get(s, 0, params, obs);
                    REQUIRE(obs.items[1][BOARD_SIZE - 3] == Item::FOG);

                    obs.VirtualStep(reconstructed, trackAgents, trackBombs, &itemAge);
                    if (print) _print_step(s, reconstructed);
                    REQUIRE(reconstructed.items[1][BOARD_SIZE - 3] == Item::PASSAGE);
                }
            }
        }
    }
}

void ClearBombOwnership(Observation& obs)
{
    for(int i = 0; i < obs.bombs.count; i++) {
        int& bomb = obs.bombs[i];
        bboard::SetBombID(bomb, bboard::AGENT_COUNT);
    }
}

TEST_CASE("TrackStats Tests", "[stats tracking]")
{
    ObservationParameters params;
    params.agentInfoVisibility = AgentInfoVisibility::OnlySelf;
    params.agentPartialMapView = false;

    Observation obs, oldObs;

    auto s = std::make_unique<bboard::State>();
    s->timeStep = 0;
    bboard::Move id = bboard::Move::IDLE;
    bboard::Move m[4] = {id, id, id, id};

    SECTION("Collect Kick")
    {
        REQUIRE(s->agents[0].canKick == false);
        s->PutAgent(0, 0, 0);
        s->PutItem(1, 0, Item::KICK);
        s->PutItem(2, 0, Item::KICK);
        s->Kill(2, 3);

        Observation::Get(*s, 1, params, obs);
        obs.TrackStats(oldObs);
        oldObs = obs;

        m[0] = Move::RIGHT;
        s->Step(m);
        REQUIRE(s->agents[0].canKick == true);

        Observation::Get(*s, 1, params, obs);
        REQUIRE(obs.agents[0].canKick == false);
        obs.TrackStats(oldObs);
        oldObs = obs;
        REQUIRE(obs.agents[0].canKick == true);

        s->Step(m);
        REQUIRE(s->agents[0].canKick == true);

        Observation::Get(*s, 1, params, obs);
        obs.TrackStats(oldObs);
        REQUIRE(obs.agents[0].canKick == true);
    }
    SECTION("Collect Range")
    {
        int initialRange = s->agents[0].bombStrength;
        REQUIRE(s->agents[0].bombStrength == initialRange);
        s->PutAgent(0, 0, 0);
        s->PutItem(1, 0, Item::INCRRANGE);
        s->PutItem(2, 0, Item::INCRRANGE);
        s->Kill(2, 3);

        Observation::Get(*s, 1, params, obs);
        obs.TrackStats(oldObs);
        oldObs = obs;

        m[0] = Move::RIGHT;
        s->Step(m);
        REQUIRE(s->agents[0].bombStrength == initialRange + 1);

        Observation::Get(*s, 1, params, obs);
        REQUIRE(obs.agents[0].bombStrength == initialRange);
        obs.TrackStats(oldObs);
        oldObs = obs;
        REQUIRE(obs.agents[0].bombStrength == initialRange + 1);

        s->Step(m);
        REQUIRE(s->agents[0].bombStrength == initialRange + 2);

        Observation::Get(*s, 1, params, obs);
        obs.TrackStats(oldObs);
        REQUIRE(obs.agents[0].bombStrength == initialRange + 2);
    }
    SECTION("Collect Extrabomb")
    {
        int initialBombs = s->agents[0].maxBombCount;
        REQUIRE(s->agents[0].maxBombCount == initialBombs);
        s->PutAgent(0, 0, 0);
        s->PutItem(1, 0, Item::EXTRABOMB);
        s->PutItem(2, 0, Item::EXTRABOMB);
        s->Kill(2, 3);

        Observation::Get(*s, 1, params, obs);
        obs.TrackStats(oldObs);
        oldObs = obs;

        m[0] = Move::RIGHT;
        s->Step(m);
        REQUIRE(s->agents[0].maxBombCount == initialBombs + 1);

        Observation::Get(*s, 1, params, obs);
        REQUIRE(obs.agents[0].maxBombCount == initialBombs);
        obs.TrackStats(oldObs);
        oldObs = obs;
        REQUIRE(obs.agents[0].maxBombCount == initialBombs + 1);

        s->Step(m);
        REQUIRE(s->agents[0].maxBombCount == initialBombs + 2);

        Observation::Get(*s, 1, params, obs);
        obs.TrackStats(oldObs);
        REQUIRE(obs.agents[0].maxBombCount == initialBombs + 2);
    }
    SECTION("Bomb count - agent places bomb")
    {
        s->PutAgent(0, 0, 0);
        s->PutAgent(9, 0, 1);
        s->Kill(2, 3);

        Observation::Get(*s, 1, params, obs);
        ClearBombOwnership(obs);
        obs.TrackStats(oldObs);
        oldObs = obs;

        // place bomb
        m[0] = bboard::Move::BOMB;
        s->Step(m);

        Observation::Get(*s, 1, params, obs);
        ClearBombOwnership(obs);

        REQUIRE(obs.bombs.count == 1);
        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == bboard::AGENT_COUNT);
        REQUIRE(obs.agents[0].bombCount == 0);
        REQUIRE(obs.agents[0].statsVisible == false);
        obs.TrackStats(oldObs);
        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == 0);
        REQUIRE(obs.agents[0].bombCount == 1);
        REQUIRE(obs.agents[0].statsVisible == true);
        oldObs = obs;

        // move right
        m[0] = bboard::Move::RIGHT;
        
        Observation::Get(*s, 1, params, obs);
        ClearBombOwnership(obs);
        
        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == bboard::AGENT_COUNT);
        REQUIRE(obs.agents[0].statsVisible == false);
        obs.TrackStats(oldObs);
        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == 0);
        REQUIRE(obs.agents[0].bombCount == 1);
        REQUIRE(obs.agents[0].statsVisible == true);
    }
    SECTION("Bomb tracking - kicking bombs")
    {
        s->agents[0].canKick = true;
        s->PutAgent(0, 0, 0);
        s->PutAgent(9, 0, 1);
        s->PutBomb(1, 0, 0, 1, 10, true);
        s->Kill(2, 3);
        // 0  b  ... 1

        // important: get initial stats from state as we artificially placed the bomb
        State initialState = *s;
        s->Step(m);
        Observation::Get(*s, 1, params, obs);
        obs.TrackStats(initialState);

        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == 0);
        REQUIRE(obs.agents[0].bombCount == 1);
        oldObs = obs;

        m[0] = bboard::Move::RIGHT;
        s->Step(m);

        // expected:
        //    0  b  ... 1
        Observation::Get(*s, 1, params, obs);
        ClearBombOwnership(obs);

        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == bboard::AGENT_COUNT);
        REQUIRE(obs.agents[0].statsVisible == false);
        obs.TrackStats(oldObs);
        REQUIRE(obs.agents[0].statsVisible == true);
        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == 0);
        REQUIRE(obs.agents[0].bombCount == 1);
        oldObs = obs;

        m[0] = bboard::Move::IDLE;
        s->Step(m);

        // expected:
        //    0     b  ... 1
        Observation::Get(*s, 1, params, obs);
        ClearBombOwnership(obs);

        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == bboard::AGENT_COUNT);
        REQUIRE(obs.agents[0].statsVisible == false);
        obs.TrackStats(oldObs);
        REQUIRE(obs.agents[0].statsVisible == true);
        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == 0);
        REQUIRE(bboard::BMB_POS_X(obs.bombs[0]) == 3);
        REQUIRE(bboard::BMB_POS_Y(obs.bombs[0]) == 0);
        REQUIRE(obs.agents[0].bombCount == 1);
    }
    SECTION("Bomb tracking - kicking moving bombs")
    {
        s->agents[0].canKick = true;
        s->PutAgent(0, 2, 0);
        s->PutAgent(9, 2, 1);
        s->PutBomb(1, 0, 0, 1, 10, true);
        Bomb &b = s->bombs[0];
        SetBombDirection(b, Direction::DOWN);
        s->Kill(2, 3);
        //    b
        //    
        // 0     ... 1


        // important: get initial stats from state as we artificially placed the bomb
        State initialState = *s;
        s->Step(m);
        Observation::Get(*s, 1, params, obs);
        obs.TrackStats(initialState);

        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == 0);
        REQUIRE(obs.agents[0].bombCount == 1);
        oldObs = obs;

        // bomb moves down, agent moves right
        m[0] = bboard::Move::RIGHT;
        s->Step(m);

        // expected:
        //
        //    
        //    0  b  ... 1
        Observation::Get(*s, 1, params, obs);
        ClearBombOwnership(obs);

        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == bboard::AGENT_COUNT);
        REQUIRE(obs.agents[0].statsVisible == false);
        obs.TrackStats(oldObs);
        REQUIRE(obs.agents[0].statsVisible == true);
        REQUIRE(bboard::BMB_ID(obs.bombs[0]) == 0);
        REQUIRE(obs.agents[0].bombCount == 1);
    }
}
