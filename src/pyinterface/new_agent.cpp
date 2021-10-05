#include "pymethods.hpp"
#include "agents.hpp"

#include <iostream>
#include "string.h"

std::unique_ptr<bboard::Agent> PyInterface::new_agent(std::string agentName, long seed)
{
    if(agentName == "SimpleAgent")
    {
        return std::make_unique<agents::SimpleAgent>(seed);
    }
    else if(agentName == "SimpleUnbiasedAgent")
    {
        return std::make_unique<agents::SimpleUnbiasedAgent>(seed);
    }
    else
    {
        return nullptr;
    }
}