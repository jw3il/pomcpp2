#include "pymethods.hpp"
#include "from_json.hpp"

#include <iostream>

// init interface state

bboard::State PyInterface::state;
bboard::Observation PyInterface::observation;
bool PyInterface::agentHasId = false;
std::unique_ptr<bboard::Agent> PyInterface::agent(nullptr);
Observation trueObs, obs, oldObs;
State trueState;

// interface methods

void _reset_state_and_obs()
{
    PyInterface::state = bboard::State();
    PyInterface::observation = bboard::Observation();
}

bool agent_create(char* agentName, long seed)
{
    auto createdAgent = PyInterface::new_agent(agentName, seed);
    if(createdAgent == nullptr)
    {
        return false;
    }

    PyInterface::agent = std::move(createdAgent);
    PyInterface::agentHasId = false;

    return true;
}

void agent_reset(int id)
{
    auto agent = PyInterface::agent.get();
    if(agent)
    {
        agent->id = id;
        agent->reset();
        _reset_state_and_obs();
        PyInterface::agentHasId = true;
    }
}

int agent_act(char* stateJson, char* obsJson, bool jsonIsState)
{
    auto agent = PyInterface::agent.get();
    if(!agent)
    {
        std::cout << "Agent does not exist!" << std::endl;
        return -1;
    }
    if(!PyInterface::agentHasId)
    {
        std::cout << "Agent has no id!" << std::endl;
        return -1;
    }

    agent->incoming.reset();
    agent->outgoing.reset();

    nlohmann::json jsonState = nlohmann::json::parse(stateJson);
    nlohmann::json jsonObs = nlohmann::json::parse(obsJson);
    PyInterface::observation = Observation();

    if(jsonIsState)
    {   
        // load real state and printout state information
        StateFromJSON(trueState, jsonState);

        ObservationParameters fullyObservable;
        Observation::Get(trueState, agent->id, fullyObservable, trueObs);

        // continue with real observation
        ObservationFromJSON(obs, jsonObs, agent->id);
        obs.TrackStats(oldObs);
        oldObs = obs;

        // check bombs
        if (trueObs.bombs.count!=obs.bombs.count){
            std::cout << "Different number of bombs! Obs/TrueState: " << obs.bombs.count << "/" << trueObs.bombs.count <<std::endl;
        }
        for(int i = trueObs.bombs.count - 1; i >= 0; i--)
        {   
            Bomb b = trueObs.bombs.queue[i];
            //search corresponding bomb in obs
            bool found = false;
            for(int j = obs.bombs.count - 1; j >= 0; j--){
                Bomb bb = obs.bombs.queue[j];
                if ((BMB_ID(b)==BMB_ID(bb)) && (BMB_POS_X(b)==BMB_POS_X(bb)) && (BMB_POS_Y(b)==BMB_POS_Y(bb))){
                    found = true;
                }
            }
            if (!found){
                std::cout << "Bomb not found: (" << BMB_POS_X(b) << "," << BMB_POS_Y(b) << "), " << BMB_ID(b) << std::endl;
            }
        }

        //check agents
        int missing_agents = 0;
        for (int i = 0; i < bboard::AGENT_COUNT; i++) {
            bboard::AgentInfo infoTrue = trueState.agents[i];
            //search corresponding agent
            bool sameAgent = false;
            for (int j = 0; j < bboard::AGENT_COUNT; j++) {
                bboard::AgentInfo info = obs.agents[j];
                if ((infoTrue.x==info.x) && (infoTrue.y==info.y)){
                    if ((infoTrue.canKick==info.canKick)&&(infoTrue.maxBombCount==info.maxBombCount)&&(infoTrue.bombStrength==info.bombStrength)){
                        sameAgent = true;
                    }
                }
            }
            if (!sameAgent){
                missing_agents += 1;
            }
        }
        if (missing_agents>0){
            std::cout << std::endl << "At least one Agent went missing (pos, canKick, maxBombCount, bombStrength)"<< std::endl;
            for (int i = 0; i < bboard::AGENT_COUNT; i++) {
                bboard::AgentInfo info = trueState.agents[i];
                if (!info.dead){
                    std::cout << "(" << info.x <<  "," << info.y <<  ") " << info.canKick << "," << info.maxBombCount << "," << info.bombStrength << " |";
                }

            }
            std::cout << "<-- true state" << std::endl;
            for (int i = 0; i < bboard::AGENT_COUNT; i++) {
                bboard::AgentInfo info = obs.agents[i];
                if (!info.dead){
                    std::cout << "(" << info.x <<  "," << info.y <<  ") " << info.canKick << "," << info.maxBombCount << "," << info.bombStrength << " |";
                }
            }
            std::cout << "<-- tracking" << std::endl;
        }
        
    }
    else
    {   
        ObservationFromJSON(PyInterface::observation, jsonObs, agent->id);
    }

    nlohmann::json msgJ = jsonObs["message"];
    if (msgJ != nlohmann::json::value_t::null)
    {
        // only receive messages when the teammate is not dead
        int teammate = jsonObs["teammate"].get<int>() - 10;
        if (!PyInterface::observation.agents[teammate].dead)
        {
            agent->incoming = std::make_unique<PythonEnvMessage>(msgJ[0], msgJ[1]);
        }
    }

    bboard::Move move = agent->act(&obs);
    // agent->Send(new PythonEnvMessage(0, 2, {agent->id, 7}));
    // bboard::PrintState(&PyInterface::state);
    // std::cout << "Agent " << agent->id << " selects action " << (int)move << std::endl;
    return (int)move;
}

void get_message(int* word0, int* word1)
{
    auto agent = PyInterface::agent.get();
    if (!agent)
    {
        std::cout << "Agent does not exist!" << std::endl;
        return;
    }

    if (agent->outgoing)
    {
        PythonEnvMessage* msg = dynamic_cast<PythonEnvMessage*>(agent->outgoing.get());
        if (msg)
        {
            if (!msg->IsValid())
            {
                std::cerr << "WARNING: Encountered invalid message " << *msg << " at agent " << agent->id << "!" << std::endl;
                return;
            }

            *word0 = msg->words[0];
            *word1 = msg->words[1];
            return;
        }
    }
    
    return;
}
