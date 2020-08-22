#include <iostream>

#include "bboard.hpp"
#include "step_utility.hpp"

namespace bboard
{

void Step(State* state, Move* moves)
{
    // do not execute step on terminal states
    if(state->finished)
        return;

    int aliveAgentsBefore = state->aliveAgents;

    ///////////////////
    //    Flames     //
    ///////////////////
    util::TickFlames(state);

    ///////////////////////
    //  Player Movement  //
    ///////////////////////

    Position oldPos[AGENT_COUNT];
    Position destPos[AGENT_COUNT];

    util::FillPositions(state, oldPos);
    util::FillDestPos(state, moves, destPos);
    util::FixDestPos(state, destPos);

    int dependency[AGENT_COUNT] = {-1, -1, -1, -1};
    int roots[AGENT_COUNT] = {-1, -1, -1, -1};

    // the amount of chain roots
    const int rootNumber = util::ResolveDependencies(state, destPos, dependency, roots);
    const bool ouroboros = rootNumber == 0; // ouroboros formation?

    int rootIdx = 0;
    int i = rootNumber == 0 ? 0 : roots[0]; // no roots -> start from 0

    // iterates 4 times but the index i jumps around the dependencies
    for(int _ = 0; _ < AGENT_COUNT; _++, i = dependency[i])
    {
        if(i == -1)
        {
            rootIdx++;
            i = roots[rootIdx];
        }
        const Move m = moves[i];

        if(state->agents[i].dead || m == Move::IDLE)
        {
            continue;
        }
        else if(m == Move::BOMB)
        {
            state->PlantBomb<true>(state->agents[i].x, state->agents[i].y, i);
            continue;
        }

        int x = state->agents[i].x;
        int y = state->agents[i].y;

        Position desired = destPos[i];

        if(util::IsOutOfBounds(desired))
        {
            continue;
        }

        int itemOnDestination = state->items[desired.y][desired.x];

        //if ouroboros, the bomb will be covered by an agent
        if(ouroboros)
        {
            for(int j = 0; j < state->bombs.count; j++)
            {
                if(BMB_POS_X(state->bombs[j]) == desired.x
                        && BMB_POS_Y(state->bombs[j]) == desired.y)
                {
                    itemOnDestination = Item::BOMB;
                    break;
                }
            }
        }

        if(IS_FLAME(itemOnDestination))
        {
            state->Kill(i, state->agents[i].GetPos());
            if(state->items[y][x] == Item::AGENT0 + i)
            {
                if(state->HasBomb(x, y))
                {
                    state->items[y][x] = Item::BOMB;
                }
                else
                {
                    state->items[y][x] = Item::PASSAGE;
                }
            }
            continue;
        }
        if(util::HasDPCollision(state, destPos, i))
        {
            continue;
        }

        //
        // All checks passed - you can try a move now
        //


        // Collect those sweet power-ups
        if(IS_POWERUP(itemOnDestination))
        {
            util::ConsumePowerup(state, i, itemOnDestination);
            itemOnDestination = Item::PASSAGE;
        }

        // execute move if the destination is free
        // (in the rare case of ouroboros, make the move even
        // if an agent occupies the spot)
        if(itemOnDestination == Item::PASSAGE ||
                (ouroboros && itemOnDestination >= Item::AGENT0))
        {
            // only override the position I came from if it has not been
            // overridden by a different agent that already took this spot
            if(state->items[y][x] == Item::AGENT0 + i)
            {
                if(state->HasBomb(x, y))
                {
                    state->items[y][x] = Item::BOMB;
                }
                else
                {
                    state->items[y][x] = Item::PASSAGE;
                }

            }

            state->items[desired.y][desired.x] = Item::AGENT0 + i;
            state->agents[i].x = desired.x;
            state->agents[i].y = desired.y;
        }
        // if destination has a bomb & the player has bomb-kick, move the player on it.
        // The idea is to move each player (on the bomb) and afterwards move the bombs.
        // If the bombs can't be moved to their target location, the player that kicked
        // it moves back. Since we have a dependency array we can move back every player
        // that depends on the inital one (and if an agent that moved there this step
        // blocked the bomb we can move him back as well).
        else if(itemOnDestination == Item::BOMB && state->agents[i].canKick)
        {
            // a player that moves towards a bomb at this(!) point means that
            // there was no DP collision, which means this agent is a root. So we can just
            // override
            if(state->HasBomb(x, y))
            {
                state->items[y][x] = Item::BOMB;
            }
            else
            {
                state->items[y][x] = Item::PASSAGE;
            }

            state->items[desired.y][desired.x] = Item::AGENT0 + i;
            state->agents[i].x = desired.x;
            state->agents[i].y = desired.y;

            // start moving the kicked bomb by setting a velocity
            // the first 5 values of Move and Direction are semantically identical
            Bomb& b = *state->GetBomb(desired.x,  desired.y);
            SetBombDirection(b, Direction(m));
        }
        else if(itemOnDestination == Item::BOMB && !state->agents[i].canKick)
        {
            if(state->HasBomb(x, y))
            {
                state->items[y][x] = Item::BOMB;
            }
            else
            {
                state->items[y][x] = Item::PASSAGE;
            }

            state->items[desired.y][desired.x] = Item::AGENT0 + i;
            state->agents[i].x = desired.x;
            state->agents[i].y = desired.y;
        }
    }

    // Before moving bombs, reset their "moved" flags
    util::ResetBombFlags(state);

    // Fill array of desired positions
    Position bombDestinations[MAX_BOMBS];
    util::FillBombDestPos(state, bombDestinations);

    // Set bomb directions to idle if they collide with an agent or a static obstacle
    for(int i = 0; i < state->bombs.count; i++)
    {
        Bomb& b = state->bombs[i];
        int bx = BMB_POS_X(b);
        int by = BMB_POS_Y(b);

        Position target = util::DesiredPosition(b);

        if(util::IsOutOfBounds(target) ||
                IS_STATIC_MOV_BLOCK(state->items[target.y][target.x]) ||
                IS_AGENT(state->items[target.y][target.x]))
        {
            SetBombDirection(b, Direction::IDLE);
            int indexAgent = state->GetAgent(bx, by);
            if(indexAgent > -1
                    && moves[indexAgent] != Move::IDLE
                    && moves[indexAgent] != Move::BOMB
                    // if the agents is where he came from he probably got bounced
                    // back to the bomb he was already standing on.
                    && !(state->agents[indexAgent].GetPos() == oldPos[indexAgent]))

            {

                util::AgentBombChainReversion(state, moves, bombDestinations, indexAgent);
                if(state->GetAgent(bx, by) == -1)
                {
                    state->items[by][bx] = Item::BOMB;
                }

            }
        }

    }

    // Move bombs
    for(int i = 0; i < state->bombs.count; i++)
    {
        Bomb& b = state->bombs[i];

        if(Move(BMB_DIR(b)) == Move::IDLE)
        {
            if(util::HasBombCollision(state, b, i))
            {
                util::ResolveBombCollision(state, moves, bombDestinations, i);
                continue;
            }
        }

        int bx = BMB_POS_X(b);
        int by = BMB_POS_Y(b);

        Position target = util::DesiredPosition(b);
        int& tItem = state->items[target.y][target.x];

        if(!util::IsOutOfBounds(target) && !IS_STATIC_MOV_BLOCK(tItem))
        {
            if(util::HasBombCollision(state, b, i))
            {
                util::ResolveBombCollision(state, moves, bombDestinations, i);
                continue;
            }

            // MOVE BOMB
            SetBombPosition(b, target.x, target.y);

            if(!state->HasBomb(bx, by) && state->items[by][bx] == Item::BOMB)
            {
                state->items[by][bx] = Item::PASSAGE;
            }

            if(IS_WALKABLE(tItem))
            {
                tItem = Item::BOMB;
            }
            else if(IS_FLAME(tItem))
            {
                state->ExplodeBombAt(state->GetBombIndex(target.x, target.y));
            }
        }
        else
        {
            SetBombDirection(b, Direction::IDLE);
        }
    }

    ///////////////
    // Explosion //
    ///////////////
    util::TickBombs(state);

    // advance timestep
    state->timeStep++;

    if(aliveAgentsBefore != state->aliveAgents)
    {
        // the number of agents has changed, check if some agent(s) won the game
        util::CheckTerminalState(*state);
    }
}

}
