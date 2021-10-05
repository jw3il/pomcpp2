#ifndef PYMETHODS_H
#define PYMETHODS_H

#include <memory>
#include "bboard.hpp"

namespace PyInterface 
{
    extern bboard::State state;
    extern bboard::Observation observation;
    extern std::unique_ptr<bboard::Agent> agent; 

    extern bool agentHasId;

    /**
     * @brief Create an agent with the type specified by the given name.
     * @param agentName A name representing the desired agent type
     * @param seed Use this seed to initialize the agent if it includes randomness
     * @return A pointer to the created agent (or nullptr)
     */
    std::unique_ptr<bboard::Agent> new_agent(std::string agentName, long seed);
}

extern "C" {
    /**
     * @brief Create an agent with the type specified by the given name.
     * @param agentName A name representing the desired agent type
     * @param seed Use this seed to initialize the agent if it includes randomness
     * @return Whether the agent has been created 
     * (only if there is an agent type for the given name)
     */
    bool agent_create(char* agentName, long seed);

    /**
     * @brief Reset the state of the agent.
     * @param id The id of the agent
     */
    void agent_reset(int id);

    /**
     * @brief Get the action from the current agent for the given state/observation.
     * @param cjson The current state/observation as a json char array
     * @param jsonIsState Whether json contains a state (true) or observation (false)
     * @return The action of the agent. -1 if there has been an error.
     */
    int agent_act(char* cjson, bool jsonIsState);

    /**
     * @brief If the agent tries to send a python-compatible message in this step, 
     * return its content using the given word pointers.
     * 
     * @param word0 Pointer to the first word (used to return value)
     * @param word1 Pointer to the second word (used to return value)
     */
    void get_message(int* word0, int* word1);
}

#endif // PYMETHODS_H
