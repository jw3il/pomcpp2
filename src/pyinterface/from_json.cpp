#include "from_json.hpp"

#include "nlohmann/json.hpp"
#include "step_utility.hpp"
#include <unordered_set>

template <typename T>
inline void _checkKeyValue(const nlohmann::json& j, const std::string& key, const T& val)
{
    if(val != j[key])
    {
        std::cout << "Full json object: " << j << std::endl;
        throw std::runtime_error("Incorrect value for " + key + "! Expected " + std::to_string(val) + ", got " + j[key].dump() + ".");
    }
}

/**
 * @brief _mapPyToBoard Maps python board integers to pomcpp Items.
 * @param py The python board integer
 * @return The pomcpp item for the given python board integer.
 */
Item _mapPyToBoard(int py)
{
    switch (py)
    {
        case 0: return Item::PASSAGE;
        case 1: return Item::RIGID;
        case 2: return Item::WOOD;
        case 3: return Item::BOMB;
        case 4: return Item::FLAME;
        case 5: return Item::FOG;
        case 6: return Item::EXTRABOMB;
        case 7: return Item::INCRRANGE;
        case 8: return Item::KICK;
        case 9: return Item::AGENTDUMMY;
        case 10: return Item::AGENT0;
        case 11: return Item::AGENT1;
        case 12: return Item::AGENT2;
        case 13: return Item::AGENT3;
        default: throw std::runtime_error("Unknown map item " + std::to_string(py));
    }
}

/**
 * @brief _mapPyToDir Maps python direction integers to pomcpp Directions
 * @param py The python direction integer
 * @return The pomcpp Direction for the given python direction integer.
 */
Direction _mapPyToDir(int py)
{
    switch (py)
    {
        case 0: return Direction::IDLE;
        case 1: return Direction::UP;
        case 2: return Direction::DOWN;
        case 3: return Direction::LEFT;
        case 4: return Direction::RIGHT;
        default: throw std::runtime_error("Unknown direction " + std::to_string(py));
    }
}

/**
 * @brief Maps a python game type to the corresponding pomcpp game mode.
 * @param py The python game type integer
 * @return The pomcpp GameMode for the given python game type.
 */
GameMode _mapPyToGameMode(int py)
{
    /* 
        FFA = 1
        Team = 2
        TeamRadio = 3
        OneVsOne = 4
    */
    switch (py)
    {
        case 1: return GameMode::FreeForAll;
        case 2: return GameMode::TwoTeams;
        default: throw std::runtime_error("Not supported game mode " + std::to_string(py));
    }
}

void _boardFromJSON(const nlohmann::json& pyBoard, State& state)
{
    for(int y = 0; y < BOARD_SIZE; y++)
    {
        for(int x = 0; x < BOARD_SIZE; x++)
        {
            state.items[y][x] = _mapPyToBoard(pyBoard[y][x].get<int>());
        }
    }
}

void _agentInfoFromJSON(const nlohmann::json& pyInfo, AgentInfo& info, int agentActiveBombsCount = 0)
{
    // attributes: agent_id, ammo, blast_strength, can_kick, is_alive, position (tuple)
    // info.team must be set outside this function
    
    // Agent positions are stored (row, column)
    info.visible = true;
    info.x = pyInfo["position"][1];
    info.y = pyInfo["position"][0];

    info.dead = !pyInfo.value("is_alive", true);

    info.statsVisible = true;
    info.canKick = pyInfo["can_kick"];
    // WARNING: Not possible to get actual bomb count from observation (?)
    info.bombCount = agentActiveBombsCount;
    info.maxBombCount = agentActiveBombsCount + pyInfo["ammo"].get<int>();
    info.bombStrength = pyInfo["blast_strength"];
}

void _bombFromJSON(const nlohmann::json& pyBomb, Bomb& bomb)
{
    // attributes: bomber_id, moving_direction, position (tuple), life, blast_strength

    SetBombID(bomb, pyBomb["bomber_id"]);
    const nlohmann::json& pos = pyBomb["position"];
    // Bomb positions are stored (row, column) in python => x is [1] and y is [0]
    SetBombPosition(bomb, pos[1], pos[0]);
    SetBombStrength(bomb, pyBomb["blast_strength"].get<int>() - 1);

    nlohmann::json movingDir = pyBomb["moving_direction"];
    if(movingDir.is_null())
    {
        SetBombDirection(bomb, Direction::IDLE);
    }
    else
    {
        SetBombDirection(bomb, _mapPyToDir(movingDir));
    }

    SetBombFlag(bomb, false);
    SetBombTime(bomb, pyBomb["life"]);
}

void _flameFromJSON(const nlohmann::json& pyFlame, Flame& flame)
{
    // attributes: position (tuple), life

    const nlohmann::json& pos = pyFlame["position"];
    // Flame positions are stored (row, column)
    flame.position.x = pos[1];
    flame.position.y = pos[0];
    // python flames stay active for one step when their value is 0
    // in their observation, + 1 is already included
    flame.timeLeft = pyFlame["life"].get<int>() + 1;
}

inline int _getTeam(GameMode gameMode, int agentID)
{
    switch (gameMode)
    {
    case GameMode::FreeForAll:
        return 0;
    case GameMode::TwoTeams:
        return (agentID % 2 == 0) ? 1 : 2;
    default:
        std::cout << "Warning: Unknown game mode mode '" << (int)gameMode << "'" << std::endl;
        return 0;
    }
}

void StateFromJSON(State& state, const std::string& json, GameMode gameMode)
{
    // attributes: board_size, step_count, board, agents, bombs, flames, items, intended_actions
    const nlohmann::json pyState = nlohmann::json::parse(json);

    _checkKeyValue(pyState, "board_size", BOARD_SIZE);

    state.timeStep = pyState["step_count"];

    // set board
    _boardFromJSON(pyState["board"], state);

    // reset bomb count
    for(uint i = 0; i < bboard::AGENT_COUNT; i++)
    {
        AgentInfo& info = state.agents[i];
        info.bombCount = 0;
    }

    // count bombs
    const nlohmann::json& pyBombs = pyState["bombs"];
    state.bombs.count = 0;
    for(uint i = 0; i < pyBombs.size(); i++)
    {
        Bomb bomb;
        _bombFromJSON(pyBombs[i], bomb);
        state.bombs.AddElem(bomb);

        // increment agent bomb count
        state.agents[BMB_ID(bomb)].bombCount++;
    }

    state.aliveAgents = 0;

    // set agents
    for(uint i = 0; i < bboard::AGENT_COUNT; i++)
    {
        AgentInfo& info = state.agents[i];
        const nlohmann::json& pyInfo = pyState["agents"][i];

        _checkKeyValue(pyInfo, "agent_id", i);

        _agentInfoFromJSON(pyInfo, info, info.bombCount);

        if(!info.dead)
        {
            state.aliveAgents++;
        }

        info.team = _getTeam(gameMode, i);

        if(!info.dead && state.items[info.y][info.x] < Item::AGENT0)
        {
            throw std::runtime_error("Expected agent, got " + std::to_string(state.items[info.y][info.x]));
        }
    }

    // set flames
    const nlohmann::json& pyFlames = pyState["flames"];
    state.flames.count = 0;
    state.currentFlameTime = -1;
    for(uint i = 0; i < pyFlames.size(); i++)
    {
        Flame flame;
        _flameFromJSON(pyFlames[i], flame);
        state.flames.AddElem(flame);

        if(!IS_FLAME(state.items[flame.position.y][flame.position.x]))
        {
            throw std::runtime_error("Invalid flame @ " + std::to_string(flame.position.x) + ", " + std::to_string(flame.position.y));
        }
    }

    // set items
    const nlohmann::json& pyItems = pyState["items"];
    for(uint i = 0; i < pyItems.size(); i++)
    {
        // attributes: tuple(position (tuple), type)
        const nlohmann::json& pyItem = pyItems[i];

        const nlohmann::json& pos = pyItem[0];
        const Item type = _mapPyToBoard(pyItem[1]);

        // Item position is (row, column)
        int& boardItem = state.items[pos[0].get<int>()][pos[1].get<int>()];
        switch (boardItem) {
            case Item::PASSAGE:
                boardItem = type;
                break;
            case Item::WOOD:
            case Item::FLAME:
                boardItem += State::ItemFlag(type);
                break;
            default:
                throw std::runtime_error("Powerup at board item " + std::to_string(boardItem));
        }
    }

    // optimize flames for faster steps
    state.currentFlameTime = util::OptimizeFlameQueue(state);
}

State StateFromJSON(const std::string& json, GameMode gameMode)
{
    State state;
    StateFromJSON(state, json, gameMode);
    return state;
}

void ObservationFromJSON(Observation& obs, const std::string& json, int agentId)
{
    nlohmann::json pyObs = nlohmann::json::parse(json);

    // attributes:
    // - game_type (int), game_env (string), step_count (int)
    // - alive (list with ids), enemies (list with ids),
    // - position (int pair), blast_strength (int), can_kick (bool), teammate (list with ids), ammo (int),
    // - board (int matrix), bomb_blast_strength (float matrix), bomb_life (float matrix), bomb_moving_direction (float matrix), flame_life (float matrix)
    
    GameMode gameMode = _mapPyToGameMode(pyObs["game_type"].get<int>());

    // we only observe our own stats and other agents are invisible by default
    // (but we may find them when iterating over the board later)
    for(int i = 0; i < AGENT_COUNT; i++) 
    {
        AgentInfo& info = obs.agents[i];

        // assume that agent is dead for now (undone for alive agents later)
        info.dead = true;

        info.team = _getTeam(gameMode, i);

        if (i == agentId) continue;

        info.visible = false;
        info.x = -i;
        info.y = -1;
        info.statsVisible = false;
    }
    
    const nlohmann::json& alive = pyObs["alive"];
    for(uint i = 0; i < alive.size(); i++)
    {
        int id = alive[i].get<int>() - 10;
        obs.agents[id].dead = false;
    }

    // we can ignore the "enemies" array because this is already known due to the team mode
    
    AgentInfo& ownInfo = obs.agents[agentId];
    // TODO: Maybe reconstruct number of own active bombs?
    _agentInfoFromJSON(pyObs, ownInfo);

    // set board

    obs.bombs.count = 0;
    obs.flames.count = 0;
    obs.currentFlameTime = -1;
    
    const nlohmann::json& pyBoard = pyObs["board"];

    for(int y = 0; y < BOARD_SIZE; y++)
    {
        for(int x = 0; x < BOARD_SIZE; x++)
        {
            Item item = _mapPyToBoard(pyBoard[y][x].get<int>());
            obs.items[y][x] = item;

            if (item == Item::FLAME)
            {
                Flame& f = obs.flames.NextPos();
                f.position.x = x;
                f.position.y = y;
                f.timeLeft = (int)pyObs["flame_life"][y][x].get<float>();
                obs.flames.count++;
            }
            else if (item >= Item::AGENT0) 
            {
                int id = item - Item::AGENT0;
                if (id != agentId) {
                    // add visibility information about this agent
                    AgentInfo& otherInfo = obs.agents[id];
                    otherInfo.visible = true;
                    otherInfo.x = x;
                    otherInfo.y = y;
                }
            }

            int life = (int)pyObs["bomb_life"][y][x].get<float>();
            if (life != 0)
            {
                Bomb& b = obs.bombs.NextPos();
                SetBombPosition(b, x, y);

                // is that necessary?
                SetBombFlag(b, false);

                // WARNING: Bomber Id is not not known! This means based on a single observation,
                // we do not know when our ammo fills back up in the future.

                // because of this, we try to reconstruct our own bombs based on the last observation
                // idea: when performing an action, remember agent ids in the last observation struct

                // TODO: This simple approximation breaks when bombs move. Maybe one could include
                // the moving direction to reconstruct agent ids of moving bombs.
                /*if(ownBombs.find({x, y}) != ownBombs.end())
                {
                    SetBombID(b, agentId);
                }
                else
                {*/
                // illegal agent id
                SetBombID(b, AGENT_COUNT);
                //}

                int blastStrength = (int)pyObs["bomb_blast_strength"][y][x].get<float>() - 1;
                SetBombStrength(b, blastStrength);

                Direction direction = _mapPyToDir((int)pyObs["bomb_moving_direction"][y][x].get<float>());
                SetBombDirection(b, direction);

                SetBombTime(b, life);

                // std::cout << "Bomb: Life " << life << ", strength: " << blastStrength << ", dir: " << (int)direction << std::endl;

                obs.bombs.count++;
            }
        }
    }

    obs.currentFlameTime = util::OptimizeFlameQueue(obs);
}

Observation ObservationFromJSON(const std::string& json, int agentId)
{
    Observation obs;
    ObservationFromJSON(obs, json, agentId);
    return obs;
}
