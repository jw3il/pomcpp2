#ifndef BBOARD_H_
#define BBOARD_H_

#include <array>
#include <string>
#include <random>
#include <memory>
#include <iostream>
#include <algorithm>
#include <functional>

namespace bboard
{

const int MOVE_COUNT  = 4;
const int AGENT_COUNT = 4;
const int BOARD_SIZE  = 11;

static_assert (BOARD_SIZE <= 15, "Board positions must fit into 8-bit");

const int BOMB_LIFETIME = 9;
const int BOMB_DEFAULT_STRENGTH = 1;

const int FLAME_LIFETIME = 3;

const int MAX_BOMBS_PER_AGENT = 5;
const int MAX_BOMBS = AGENT_COUNT * MAX_BOMBS_PER_AGENT;

/**
 * Holds all moves an agent can make on a board. An array
 * of 4 moves are necessary to correctly calculate a full
 * simulation step of the board
 * @brief Represents an atomic move on a board
 */
enum class Move
{
    IDLE = 0,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    BOMB
};

enum class Direction
{
    IDLE = 0,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

enum Item
{
    PASSAGE    = 0,
    RIGID      = 1,
    WOOD       = 2 << 8,
    BOMB       = 3,
    // optimization I in docs
    FLAME      = 4 << 16,
    FOG        = 5,
    EXTRABOMB  = 6,
    INCRRANGE  = 7,
    KICK       = 8,
    AGENTDUMMY = 9,
    AGENT0 = (1 << 24),
    AGENT1 = (1 << 24) + 1,
    AGENT2 = (1 << 24) + 2,
    AGENT3 = (1 << 24) + 3
};

inline bool IS_WOOD(int x)
{
    return (((x) >> 8) == 2);
}
inline bool IS_POWERUP(int x)
{
    return ((x) > 5 && (x) < 9);
}
inline bool IS_WALKABLE(int x)
{
    return (IS_POWERUP((x)) || (x) == 0);
}
inline bool IS_FLAME(int x)
{
    return (((x) >> 16) == 4);
}
inline bool IS_AGENT(int x)
{
    return ((x) >= (1 << 24));
}
// bombs can't move through the following `static` objects: walls, boxes and upgrades.
inline bool IS_STATIC_MOV_BLOCK(int x)
{
    return (IS_WOOD((x)) || IS_POWERUP((x)) || ((x) == 1));
}
inline int FLAME_ID(int x)
{
    return (((x) & 0xFFFF) >> 3);
}
inline int FLAME_POWFLAG(int x)
{
    return ((x) & 0b11);
}
inline int WOOD_POWFLAG(int x)
{
    return ((x) & 0b11);
}
inline int CLEAR_POWFLAG(int x)
{
    return ((x) & ~0b11);
}

/**
 * @brief The FixedQueue struct implements a fixed-size queue,
 * operating on a cicular buffer.
 */
template<typename T, int TSize>
struct FixedQueue
{
    T queue[TSize];
    int index = 0;
    int count = 0;

    int RemainingCapacity()
    {
        return TSize - count;
    }

    /**
     * @brief PopBomb Frees up the position of the elem in the
     * queue to be used by other elems.
     */
    T& PopElem()
    {
        int x = index;
        index = (index + 1) % (TSize);
        count--;
        return queue[x % TSize];
    }

    /**
     * @brief AddElem Adds an element to the queue
     */
    void AddElem(const T& elem)
    {
        NextPos() = elem;
        count++;
    }

    /**
     * @brief AddElem Adds an element at a specified index
     * @param elem The element which should be added
     * @param at The index inside the list where the element should be added (must be in [0, count])
     * Highly discouraged! Only use if necessary
     */
    void AddElem(const T& elem, const int at)
    {
        // shift all elements by 1 from the end until at
        for(int i = count - 1; i >= at; i--)
        {
            int translatedIndex = (index + i) % TSize;
            queue[(translatedIndex + 1 + TSize) % TSize] = queue[translatedIndex];
        }

        // finally add the element at the specified position
        queue[(index + at) % TSize] = elem;

        count++;
    }

    /**
     * @brief RemoveAt Removes an element at a specified index
     * Highly discouraged! Only use if necessary
     */
    void RemoveAt(int removeAt)
    {
        for(int i = removeAt + 1; i < count; i++)
        {
            int translatedIndex = (index + i) % TSize;
            queue[(translatedIndex - 1 + TSize) % TSize] = queue[translatedIndex];
        }
        count--;
    }

    /**
     * @brief PollNext Polls the next free queue spot
     */
    T& NextPos()
    {
        return queue[(index + count) % TSize];
    }

    /**
     * @brief CopyTo Copies the elements of this queue to an array.
     * @param arr The array which will contain the elements of this queue.
     */
    void CopyTo(T arr[])
    {
        if(count == 0)
            return;

        int queueEndIndex = (index + count) % TSize;
        if(queueEndIndex > index)
        {
            // there are no partitions, just copy the content
            std::copy_n(&queue[index], count, &arr[0]);
        }
        else
        {
            // otherwise: there are two partitions
            int firstPartitionSize = TSize - index;
            std::copy_n(&queue[index], firstPartitionSize, &arr[0]);
            std::copy_n(&queue[0], queueEndIndex, &arr[firstPartitionSize]);
        }
    }

    /**
     * @brief CopyFrom Copies the elements from the given array into this queue.
     * @param arr The queue will be initialized with the elements of this array.
     */
    void CopyFrom(const T arr[], const int size)
    {
        index = 0;
        count = size;
        std::copy_n(&arr[0], count, &queue[0]);
    }

    /**
     * @brief operator [] Circular buffer on all bombs
     * @return The i-th elem if the index is in [0, n]
     * where n = elemcount, sorted w.r.t. their lifetime
     */
    T&       operator[] (const int index);
    const T& operator[] (const int index) const;
};

template<typename T, int TSize>
inline T& FixedQueue<T, TSize>::operator[] (const int offset)
{
    return queue[(index + offset) % TSize];
}
template<typename T, int TSize>
inline const T& FixedQueue<T, TSize>::operator[] (const int offset) const
{
    return queue[(index + offset) % TSize];
}
template<typename T, int TSize>
inline std::ostream & operator<<(std::ostream & str, const FixedQueue<T, TSize>& q)
{
    for(int i = 0; i < q.count; i++)
    {
        str << "'" << q[i] << "'";
        if(i < q.count - 1)
        {
            str << ", ";
        }
    }
    return str;
}

/**
 * @brief Represents any position on a board of a state
 */
struct Position
{
    int x;
    int y;

    Position(): x(0), y(0) {}
    Position(int x, int y): x(x), y(y) {}
};

inline Position operator+(const Position& a, const Position& b)
{
    return Position(a.x + b.x, a.y + b.y);
}

inline Position operator-(const Position& a, const Position& b)
{
    return Position(a.x - b.x, a.y - b.y);
}

inline bool operator==(const Position& a, const Position& b)
{
    return a.x == b.x && a.y == b.y;
}

inline bool operator!=(const Position& a, const Position& b)
{
    return a.x != b.x || a.y != b.y;
}

inline std::ostream & operator<<(std::ostream& str, const Position& p)
{
    str << "(" << p.x << ", " << p.y << ")";;
    return str;
}

inline bool InViewRange(int x1, int y1, int x2, int y2, int range)
{
    return std::abs(x1 - x2) <= range && std::abs(y1 - y2) <= range;
}

inline bool InViewRange(const Position& p1, int x2, int y2, int range)
{
    return InViewRange(p1.x, p1.y, x2, y2, range);
}

inline bool InViewRange(const Position& p1, const Position& p2, int range)
{
    return InViewRange(p1.x, p1.y, p2.x, p2.y, range);
}

/**
 * @brief The AgentInfo struct holds information ABOUT
 * an agent.
 *
 * - Why not put it in the Agent struct?
 * Because the Agent struct is a virtual base that implements
 * the behaviour of an agent. We might want to hotswap agent
 * behaviours during the game, without having to worry about
 * copying all variables.
 *
 * The act-method is not relevant to the game's mechanics, so
 * it's excluded for now
 *
 * - Why not use an array of vars in the State struct instead?
 * That was the first approach, however fogging the state is a
 * lot easier if all (possibly hidden) data is bundled. Now if
 * someone is out of sight we simply don't expose their AgentInfo
 * to the agent.
 */
struct AgentInfo
{
    // public information

    /**
     * @brief team An id for the team this agents belongs to.
     * The value 0 represents that the agent is in NO team.
     */
    int team = 0;

    /**
     * @brief Whether this agent is dead. 
     */
    bool dead = false;

    /**
     * @brief If an agent is not visible, its position (x, y) is unknown.
     */
    bool visible = true;
    int x;
    int y;

    // private information (in observations only known in special cases, e.g. for the own agent)

    /**
     * @brief Whether the stats (bombCount, maxBombCount, bombStrength, canKick) of this agent are visible.
     */
    bool statsVisible = true;

    /**
     * @brief The number of active bombs placed by this agent.
     */
    int bombCount = 0;

    /**
     * @brief The maximal number of bombs this agent can place simultaneously.
     */
    int maxBombCount = 1;

    /**
     * @brief The strength = range of bombs in cells.
     */
    int bombStrength = BOMB_DEFAULT_STRENGTH;

    /**
     * @brief Whether this agent can kick bombs.
     */
    bool canKick = false;

    inline Position GetPos() const
    {
        return {x, y};
    }

    /**
     * @brief Checks whether the other agent is an enemy of this agent.
     * @param other The other agent.
     * @return Whether the given agent is an enemy of this agent. 
     */
    inline bool IsEnemy(const AgentInfo& other) const
    {
        return team == 0 || other.team != team; 
    }
};

/**
 * Represents all information about a single
 * bomb on the board.
 *
 * Specification (see docs optimization II)
 *
 *   Bit     Semantics
 * [ 0,  4]  x-Position
 * [ 4,  8]  y-Position
 * [ 8, 12]  ID
 * [12, 16]  Strength
 * [16, 20]  Time
 * [20, 24]  Direction
 */
typedef int Bomb;

// BOMB INFO
// ACCESS ALL PARTS OF THE BOMB INTEGER (EVERYTHING ENCODED
// INTO 4 BIT WIDE FIELDS)
inline int BMB_POS_X(const Bomb x)
{
    return (((x) & 0xF));             // [ 0, 4[
}
inline int BMB_POS_Y(const Bomb x)
{
    return (((x) & 0xF0) >> 4);       // [ 4, 8[
}
inline Position BMB_POS(const Bomb x)
{
    return (Position){BMB_POS_X(x), BMB_POS_Y(x)};
}
inline int BMB_ID(const Bomb x)
{
    return (((x) & 0xF00) >> 8);      // [ 8,12[
}
inline int BMB_STRENGTH(const Bomb x)
{
    return (((x) & 0xF000) >> 12);    // [12,16[
}
inline int BMB_TIME(const Bomb x)
{
    return (((x) & 0xF0000) >> 16);   // [16,20[
}
inline int BMB_DIR(const Bomb x)
{
    return (((x) & 0xF00000) >> 20);  // [20,24[
}
inline int BMB_FLAG(const Bomb x)
{
    return (((x) & 0xF000000) >> 24); // [24,28[
}

// inverted bit-mask
const int cmask0_4   =  ~0xF;
const int cmask4_8   =  ~0xF0;
const int cmask8_12  =  ~0xF00;
const int cmask12_16 =  ~0xF000;
const int cmask16_20 =  ~0xF0000;
const int cmask20_24 =  ~0xF00000;
const int cmask24_28 =  ~0xF000000;

inline void ReduceBombTimer(Bomb& bomb)
{
    bomb = bomb - (1 << 16);
}
inline void SetBombPosition(Bomb& bomb, int x, int y)
{
    bomb = (bomb & cmask0_4 & cmask4_8) + (x) + (y << 4);
}
inline void SetBombPosition(Bomb& bomb, Position pos)
{
    SetBombPosition(bomb, pos.x, pos.y);
}
inline void SetBombID(Bomb& bomb, int id)
{
    bomb = (bomb & cmask8_12) + (id << 8);
}
inline void SetBombStrength(Bomb& bomb, int strength)
{
    bomb = (bomb & cmask12_16) + (strength << 12);
}
inline void SetBombTime(Bomb& bomb, int time)
{
    bomb = (bomb & cmask16_20) + (time << 16);
}
inline void SetBombDirection(Bomb& bomb, Direction dir)
{
    bomb = (bomb & cmask20_24) + (int(dir) << 20);
}
inline void SetBombFlag(Bomb& bomb, bool moved)
{
    bomb = (bomb & cmask24_28) + (int(moved) << 24);
}

/**
 * @brief The Flame struct holds all information about a single flame
 * on the board.
 */
struct Flame
{
    Position position;
    int timeLeft;
    int destroyedWoodAtTimeStep = -1;
};

inline std::ostream & operator<<(std::ostream & str, const Flame& f)
{
    str << "(p: " << f.position << ", t: " << f.timeLeft << ", w: " << f.destroyedWoodAtTimeStep << ")";;
    return str;
}

/**
 * @brief Holds all information about the game board.
 */
class Board
{
public:
    /**
     * @brief items Holds all items on this board. Additional information for
     * bombs and flames is stored in separate queues.
     */
    int items[BOARD_SIZE][BOARD_SIZE];

    /**
     * @brief agents Array of all agents and their properties
     */
    AgentInfo agents[AGENT_COUNT];

    /**
     * @brief bombQueue Holds all bombs on this board
     */
    FixedQueue<Bomb, MAX_BOMBS> bombs;

    /**
     * @brief flames Holds all flames on this board.
     */
    FixedQueue<Flame, BOARD_SIZE * BOARD_SIZE> flames;

    /**
     * @brief timeStep The current timestep. -1 if unknown.
     */
    int timeStep = -1;

    /**
     * @brief currentFlameTime The max flameTime of all flames alive.
     * Used for the optimized flame queue. -1 if flame queue is not optimized.
     */
    int currentFlameTime = -1;

    /**
     * @brief CopyFrom Copies the elements from the given board object to this board.
     * @param board The board which should be copied.
     * @param copyAgents Whether to copy the AgentInfo.
     */
    void CopyFrom(const Board& board, bool copyAgents = true);

    /**
     * @brief Checks whether the agents with the given ids are enemies.
     * @param agentA The id of the first agent.
     * @param agentB The id of the second agent.
     * @return Whether the agents are enemies.
     */
    inline bool Enemies(int agentA, int agentB) const
    {
        return agents[agentA].IsEnemy(agents[agentB]);
    }

    /**
     * @brief PutItem Places an item on the board
     */
    inline void PutItem(int x, int y, Item item)
    {
        items[y][x] = item;
    }

    /**
     * @brief Clear Overrides all items on the board with the given item.
     * @param item The item which will be used to clear the board.
     */
    inline void Clear(const Item item = Item::PASSAGE)
    {
        std::fill_n(&items[0][0], BOARD_SIZE * BOARD_SIZE, item);
    }

    /**
     * @brief HasAgent Returns the index of the agent that occupies
     * the given position. -1 if no agent is there
     */
    int GetAgent(int x, int y) const;

    /**
     * @brief Places agents with given IDs clockwise in the corners of
     * the board, starting from top left.
     * 
     * @param a0 The id of the first agent.
     * @param a1 The id of the second agent.
     * @param a2 The id of the third agent.
     * @param a3 The id of the fourth agent.
     * @param padding The padding to the walls (>= 0).
     */
    void PutAgentsInCorners(int a0, int a1, int a2, int a3, int padding);

    /**
     * @brief Places a specified agent on the specified location and updates 
     * agent positions.
     * 
     * @param x x-position of the agent.
     * @param y y-position of the agent.
     * @param agentID The agent ID (from 0 to AGENT_COUNT)
     */
    void PutAgent(int x, int y, int agentID);

    /**
     * @brief Puts a bomb at the specified position with given strength and lifetime.
     * Adds one to the agent's bomb count.
     * 
     * @param x x-position of the agent
     * @param y y-position of the agent
     * @param agentID ID of the agent that "owns" this bomb
     * @param strength Bomb strength
     * @param lifeTime Bomb lifetime
     * @param setItem Whether to set the board item to Item::BOMB 
     */
    void PutBomb(int x, int y, int agentID, int strength, int lifeTime, bool setItem);

    /**
     * @brief hasBomb Returns true if a bomb is at the specified
     * position
     */
    bool HasBomb(int x, int y) const;

    /**
     * @brief GetBomb Returns a bomb at the specified location or 0 if
     * no bomb has that position
     */
    Bomb* GetBomb(int x, int y) const;

    /**
     * @brief GetBombIndex If a bomb is at position (x,y), then
     * returns the index of the bomb in the bomb queue. -1 otherwise
     */
    int GetBombIndex(int x, int y) const;

    /**
     * @brief FlagItem Returns the correct powerup
     * for the given pow-flag
     */
    static Item FlagItem(int powFlag);

    /**
     * @brief ItemFlag Returns the correct pow-flag
     * for the given item
     */
    static int ItemFlag(Item item);

    /**
     * @brief Prints the board into the standard output stream.
     * 
     * @param clearConsole Whether to clear the console before printing
     */
    virtual void Print(bool clearConsole = false) const;
};

/**
 * @brief The GameMode used in the environment.
 */
enum class GameMode
{
    FreeForAll = 0,
    TwoTeams,
    TeamRadio,
};

/**
 * @brief Gets the team of the given agent according to the game mode.
 * @param gameMode The game mode
 * @param agentID An agent id
 */
inline int GetTeam(GameMode gameMode, int agentID)
{
    switch (gameMode)
    {
    case GameMode::FreeForAll:
        return 0;
    case GameMode::TwoTeams:
    case GameMode::TeamRadio:
        return (agentID % 2 == 0) ? 1 : 2;
    default:
        std::cout << "Warning: Unknown game mode mode '" << (int)gameMode << "'" << std::endl;
        return 0;
    }
}

/**
 * @brief Gets the ID of the teammate in four player team game modes.
 * @param agentID An agent id
 */
inline int GetTeammateID(int agentID)
{
    return (agentID + 2) % 4;
}

/**
 * @brief SetTeams Sets the teams of the agents according to the given game mode.
 * @param agents The agent infos
 * @param gameMode The game mode
 */
void SetTeams(AgentInfo agents[AGENT_COUNT], GameMode gameMode);

/**
 * @brief Holds all information about the game state and provides member functions to initialize the board and execute steps.
 */
class State : public Board
{
public:
    /**
     * @brief finished Whether this is a terminal state.
     */
    bool finished = false;

    /**
     * @brief isDraw Whether the game resulted in a draw.
     */
    bool isDraw = false;

    /**
     * @brief winningTeam The winning team (0 if no team has won)
     */
    int winningTeam = 0;

    /**
     * @brief winningAgent The single winning agent (-1 if no agent has won
     * or when the winning agents are in a team).
     */
    int winningAgent = -1;

    /**
     * @brief aliveAgents The number of alive agents (same as sum([!a.dead for a in agents])
     */
    int aliveAgents = AGENT_COUNT;

    /**
     * @brief Init Initializes the state and puts boxes, rigid objects, powerups and agents on the board.
     * @param boardSeed The random seed for the item generator.
     * @param agentPositionSeed When != -1, randomly set the agent positions using this seed. Otherwise, set the start positions clockwise (0 = top left).
     * @param numRigid The number of rigid blocks on the board.
     * @param numWood The number of wooden blocks on the board.
     * @param numItems The number of powerups on the board (must be <= numWood).
     * @param padding The padding of the agents to the walls.
     * @param breathingRoomSize The size of the "breathing room" between agents.
     */
    void Init(GameMode gameMode, long boardSeed, long agentPositionSeed, int numRigid = 36, int numWood = 36, int numPowerUps = 20, int padding = 1, int breathingRoomSize = 3);

    /**
     * @brief Execute a step using the given moves.

     * @param moves The agents' moves
     */
    void Step(Move* moves);

    /**
     * @brief Puts a bomb at the agent's position if it has enough available bombs.
     */
    template<bool duringStep>
    inline void TryPutBomb(int id, bool setItem = false)
    {
        AgentInfo& agent = agents[id];
        if(agent.bombCount >= agent.maxBombCount || HasBomb(agent.x, agent.y))
            return;

        // when we plant a bomb during the step function, we need to increment the bomb lifetime by 1
        // because all bomb lifetimes will be decremented at the end of this step
        PutBomb(agent.x, agent.y, id, agent.bombStrength, BOMB_LIFETIME + (duringStep ? 1 : 0), setItem);
    }

    /**
     * @brief ExplodeTopBomb Explodes the bomb at the the specified index
     * of the queue and spawns flames.
     */
    void ExplodeBombAt(int index);

    /**
     * @brief SpawnFlameItem Spawns a single flame item on the board
     * @param x The x position of the fire
     * @param y The y position of the fire
     * @param isCenterFlame Whether (x, y) is the bomb's position
     * @return Should we continue spawing flames in that direction?
     */
    bool SpawnFlameItem(int x, int y, bool isCenterFlame);

    /**
     * @brief SpawnFlames Spawns rays of flames at the
     * specified location.
     * @param x The x position of the origin of flames
     * @param y The y position of the origin of flames
     * @param strength The farthest reachable distance
     * from the origin
     */
    void SpawnFlames(int x, int y, int strength);

    /**
     * @brief PopFlame extinguishes all the top flames
     * of the flame queue which have zero lifetime left.
     */
    void PopFlames();

    /**
     * @brief Checks whether the given agent has won.
     * @param agentID The agent ID
     * @return Whether the agent has won
     */
    bool IsWinner(int agentID) const;

    /**
     * Kills all listed agents.
     */
    template<typename... Args>
    void Kill(int agentID, Args... args)
    {
         Kill(agentID);
         Kill(args...);
    }
    void Kill();

    /**
     * @brief Kill Kill some agent on this board.
     * @param agentID The id of the agent
     */
    void Kill(const int agentID);

    /**
     * @brief EventBombExploded Called when a bomb explodes.
     * @param b The bomb which explodes.
     */
    void EventBombExploded(Bomb b);


    void Print(bool clearConsole = false) const override;
};

/**
 * @brief Defines which agent information is in an observation.
 */
enum class AgentInfoVisibility
{
    All,
    InView,
    OnlySelf
};

/**
 * @brief Parameters which define how agents observe the environment.
 */
struct ObservationParameters
{
    /**
     * @brief Whether the observation includes meta information about other agents.
     */
    AgentInfoVisibility agentInfoVisibility = AgentInfoVisibility::All;

    /**
     * @brief Whether to include powerup information hidden inside wooden boxes.
     */
    bool exposePowerUps = true;

    /**
     * @brief Whether to limit the view of the agents according to agentViewSize.
     */
    bool agentPartialMapView = false;

    /**
     * @brief The number of blocks the agents can see in each direction.
     */
    int agentViewSize = 4;
};

/**
 * @brief The observation of an agent.
 */
class Observation : public Board
{
public:
    int agentID;
    ObservationParameters params;

    /**
     * @brief Creates an observation for some agent based on a state.
     * 
     * @param state The current state of the environment
     * @param agentID The id of the agent for which the observation is created
     * @param obsParams The parameters which define which information the observation will contain
     * @param observation The object which will be used to save the observation
     */
    static void Get(const State& state, const uint agentID, const ObservationParameters obsParams, Observation& observation);

    /**
     * @brief Converts this observation to a (potentially incomplete) state. This allows you to execute steps on that observation. 
     * If an agent's stats are not visible in this observation, the stats from the given state are used instead. 
     * 
     * @param state The state object that will be used to save the state.
     */
    void ToState(State& state) const;

    /**
     * @brief Execute a virtual step by merging this observation (timestep t+1) into the given state (timestep t).
     * Tracks previously seen items and can keep agents and bombs depending on the given arguments.
     * 
     * Warning: is an EXPERIMENTAL heuristic.
     * 
     * @param state The last state that will be updated with the information from this observation.
     * @param keepAgents Whether to keep old agents (also removes duplicates)
     * @param keepBombs Whether to keep old bombs (DOES NOT remove duplicates!)
     * @param itemAge Can be used to keep track of age of all items (#steps since last update)
     */
    void VirtualStep(State& state, bool keepAgents, bool keepBombs, int (*itemAge)[BOARD_SIZE][BOARD_SIZE] = nullptr) const;

    /**
     * @brief Updates agent stats and bomb ownership in this observation using given board information from the last step.
     * 
     * Warning: is a simple heuristic. Even in the FFA environment, it is impossible to track all stats in all cases.
     * 
     * @param oldBoard The last board holding previous state information (can be observation or state).
     */
    void TrackStats(const Board& oldBoard);

    void Print(bool clearConsole = false) const override;
};

/**
 * @brief Superclass for messages that can be transmitted between agents.
 */
class Message
{
public:
    virtual ~Message() = default;

    /**
     * @brief Clone this message.
     * 
     * @return A clone of this message. 
     */
    virtual std::unique_ptr<Message> clone() const = 0;
};

/**
 * @brief A message that consists of multiple words of type T.
 * 
 * @tparam T Word type
 * @tparam nbWords Number of words
 */
template<typename T, std::size_t nbWords>
class MultiWordMessage : public Message
{
public:
    const std::array<T, nbWords> words;
    MultiWordMessage(std::array<T, nbWords> words) : words(words) {}

    std::unique_ptr<Message> clone() const
    {
        return std::make_unique<MultiWordMessage<T, nbWords>>(words);
    }
};

/**
 * @brief A message that is compatible to the messages used in the python environment. Consists of two numbers from 0 to 7.
 */
class PythonEnvMessage : public MultiWordMessage<int, 2>
{
public:
    /**
     * @brief Construct a new PythonEnvMessage.
     * 
     * @param words An array with two words.
     */
    PythonEnvMessage(std::array<int, 2> words) : MultiWordMessage(words) {}

    /**
     * @brief Construct a new PythonEnvMessage with individual words.
     * 
     * @param word0 The first word
     * @param word1 The second word
     */
    PythonEnvMessage(int word0, int word1) : MultiWordMessage({word0, word1}) {}

    /**
     * @brief Checks whether both words contain valid values from 0 to 7 (inclusive).
     * 
     * @return Whether this message is valid.
     */
    bool IsValid();
};

std::ostream& operator<<(std::ostream &ostream, const PythonEnvMessage &msg);

/**
 * @brief The agent class defines the behaviour of an agent.
 */
class Agent
{
public:
    virtual ~Agent() {}

    /**
     * @brief The agent's id.
     */
    int id = -1;

    /**
     * @brief Incoming messages are placed here.
     */
    std::unique_ptr<Message> incoming;

    /**
     * @brief Outgoing messages should be placed here. 
     */
    std::unique_ptr<Message> outgoing;

    /**
     * @brief Send a new PythonEnvMessage.
     * 
     * @param word0 The first word (0 <= word0 <= 7)
     * @param word1 The second word (0 <= word1 <= 7)
     */
    inline void SendMessage(int word0, int word1)
    {
        outgoing = std::make_unique<PythonEnvMessage>(word0, word1);
    }

    /**
     * @brief Try to read a message of a given type from the incoming messages.
     * @return A pointer to the message (nullptr if there is no message or if it is of a different type)
     */
    inline PythonEnvMessage* TryReadMessage()
    {
        return dynamic_cast<PythonEnvMessage*>(incoming.get());
    }

    /**
     * This method defines the behaviour of the Agent.
     * Classes that implement Agent can be used to participate
     * in a game and run.
     *
     * @brief For a given observation, return a Move
     * 
     * @param obs The observation
     * @return A Move (integer, 0-..)
     */
    virtual Move act(const Observation* obs) = 0;

    /**
     * @brief Reset the state of this agent for a new episode.
     */
    virtual void reset();

    /**
     * @brief Copy the given agent (clones messages).
     * 
     * @param other The other agent that should be copied.
     * @return This agent 
     */
    Agent& operator=(const Agent& other)
    {
        id = other.id;
        // create message clones (direct copy not allowed)
        incoming = other.incoming ? other.incoming->clone() : nullptr;
        outgoing = other.outgoing ? other.outgoing->clone() : nullptr;
        return *this;
    }
};

/**
 * @brief The Environment struct holds all information about a
 * Game (current state, participating agents) and takes care of
 * distributing observations to the correct agents.
 */
class Environment
{

private:
    std::unique_ptr<State> state;
    std::array<Agent*, AGENT_COUNT> agents;
    std::array<Observation, AGENT_COUNT> observations;

    std::function<void(const Environment&)> listener;

    GameMode gameMode;
    ObservationParameters observationParameters;
    bool communication = false;

    // Current State
    bool hasStarted = false;
    bool hasFinished = false;

    bool threading = false;
    int threadCount = 1;

    Move lastMoves[AGENT_COUNT];
    bool hasActed[AGENT_COUNT];

public:

    Environment();

    /**
     * @brief MakeGame Initializes the state
     */
    void MakeGame(std::array<Agent*, AGENT_COUNT> a, GameMode gameMode = GameMode::FreeForAll, long boardSeed = 0x1337, long agentPositionSeed = -1);

    /**
     * @brief Sets the observation parameters that are used to create the observations for all agents.
     */
    void SetObservationParameters(ObservationParameters parameters);

    /**
     * @brief RunGame simulates some steps in the game.
     * (blocking)
     * @param steps The number of simulated steps. The environment will run until IsDone() for values <= 0.
     * @param asyncMoves Whether to collect the moves asynchronously
     * @param render True if the game should be rendered
     * @param renderClear Whether to clear the console output after each frame
     * @param renderInteractive Whether to wait for user input at each rendered frame
     * @param renderWaitMs The amount of ms to wait after each frame has been rendered
     */
    void RunGame(int steps, bool asyncMoves = false, bool render = false, bool renderClear = false, bool renderInteractive = false, int renderWaitMs = 100);

    /**
     * @brief Step Executes a step, given by the params
     * @param competitiveTimeLimit Set to true if the agents
     * need to produce a response in less than 100ms (competition
     * rule). Timed out agents will have the IDLE move.
     */
    void Step(bool competitiveTimeLimit = false);

    /**
     * @brief Print Pretty-prints the Environment
     * @param clear Should the console be cleared first?
     */
    void Print(bool clear = true);

    /**
     * @brief PrintGameResult Prints the result of the game.
     */
    void PrintGameResult();

    /**
     * @brief GetState Returns a reference to the current state of
     * the environment
     */
    State& GetState() const;

    /**
     * @brief GetGameMode Returns the current game mode.
     */
    GameMode GetGameMode() const;

    /**
     * @brief Updates the observation of the given agent and returns a pointer to it.
     */
    const Observation* GetObservation(uint agentID);

    /**
     * @brief SetAgents Registers all agents that will participate
     * in this game
     * @param a An array of agent pointers (with correct length)
     * the team assignment
     */
    void SetAgents(std::array<Agent*, AGENT_COUNT> agents);

    /**
     * @brief GetAgent
     * @param agentID
     * @return
     */
    Agent* GetAgent(uint agentID) const;

    /**
     * @brief SetStepListener Sets the step listener. Step listener
     * gets invoked every time after the step function was called.
     */
    void SetStepListener(const std::function<void(const Environment&)>& f);

    /**
     * @return True if the last step ended the current game
     */
    bool IsDone();

    /**
     * @brief IsDraw Did the game end in a draw?
     */
    bool IsDraw();

    /**
     * @brief GetUniqueWinner returns the id of the agent who won the game (if that agent is in no team).
     * -1 if there is no unique winner without a team.
     */
    int GetWinningAgent();

    /**
     * @brief GetWinningTeam returns the winning team (0 if no team has won the game).
     */
    int GetWinningTeam();

    /**
     * @brief GetLastMove Returns the last move made by the given agent.
     * (can be old if the agent was dead, use in combination with HasActed)
     * @param agentID The id of the agent.
     */
    Move GetLastMove(int agentID);

    /**
     * @brief HasActed Returns whether the given agent executed a move in the last step.
     * @param agentID The id of the agent.
    */
    bool HasActed(int agentID);
};

/**
 * @brief Returns a string, corresponding to the given item
 * @return A 3-character long string
 */
std::string PrintItem(int item);

}

namespace std
{
template<>
struct hash<bboard::Position>
{
    size_t
    operator()(const bboard::Position & obj) const
    {
        return hash<int>()(obj.x + obj.y * bboard::BOARD_SIZE);
    }
};
}

#endif
