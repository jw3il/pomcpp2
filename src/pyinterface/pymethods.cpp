#include "pymethods.hpp"
#include "from_json.hpp"

#include <iostream>

// init interface state

bboard::State PyInterface::state;
bboard::Observation PyInterface::observation;
bool PyInterface::agentHasId = false;
std::unique_ptr<bboard::Agent> PyInterface::agent(nullptr);

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

    PyInterface::agent.reset(createdAgent);
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

int agent_act(char* cjson, bool jsonIsState)
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

    agent->incoming.release();
    agent->outgoing.release();

    nlohmann::json json = nlohmann::json::parse(cjson);
    // std::cout << "json > " << json << std::endl;
    if(jsonIsState)
    {
        StateFromJSON(PyInterface::state, json);

        ObservationParameters fullyObservable;
        Observation::Get(PyInterface::state, agent->id, fullyObservable, PyInterface::observation);
    }
    else
    {
        ObservationFromJSON(PyInterface::observation, json, agent->id);
    }

    nlohmann::json msgJ = json["message"];
    if (msgJ != nlohmann::json::value_t::null)
    {
        int teammate = json["teammate"].get<int>() - 10;
        if (!PyInterface::observation.agents[teammate].dead)
        {
            auto message = std::make_unique<PythonEnvMessage>(msgJ[0], msgJ[1]);
            // std::cout << "Msg: " << *message << std::endl;
            agent->incoming = std::move(message);
        }
    }

    bboard::Move move = agent->act(&PyInterface::observation);
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
