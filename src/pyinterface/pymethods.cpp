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

    agent->inbox.clear();
    agent->outbox.clear();

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
            auto message = new bboard::PythonEnvMessage(teammate, agent->id, {msgJ[0].get<int>(), msgJ[1].get<int>()});
            // std::cout << "Msg: " << *message << std::endl;
            agent->Receive(message);
        }
    }

    bboard::Move move = agent->act(&PyInterface::observation);
    // agent->Send(new PythonEnvMessage(0, 2, {agent->id, 7}));
    // bboard::PrintState(&PyInterface::state);
    // std::cout << "Agent " << agent->id << " selects action " << (int)move << std::endl;
    return (int)move;
}

void get_message(int* content_0, int* content_1)
{
    auto agent = PyInterface::agent.get();
    if (!agent)
    {
        std::cout << "Agent does not exist!" << std::endl;
        return;
    }

    for (int i = 0; i < agent->outbox.size(); i++)
    {
        PythonEnvMessage* msg = agent->TryReadOutbox<PythonEnvMessage>(i);
        if (msg != nullptr)
        {
            *content_0 = msg->content[0];
            *content_1 = msg->content[1];
            return;
        }
    }
    
    return;
}
