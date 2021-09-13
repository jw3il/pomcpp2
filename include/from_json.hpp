#ifndef FROM_JSON_H
#define FROM_JSON_H

#include "bboard.hpp"
#include "nlohmann/json.hpp"

using namespace bboard;

/**
 * @brief StateFromJSON Converts a state string from python to a state.
 * @param state The state reference used to save the state
 * @param json The state (string or json)
 */
void StateFromJSON(State& state, const nlohmann::json& json);

/**
 * @brief StateFromJSON Converts a state string from python to a state.
 * @param json The state (string or json)
 * @return The state created from the string
 */
State StateFromJSON(const nlohmann::json& json);

/**
 * @brief ObservationFromJSON Converts an observation from python to an observation.
 * @param obs The observation reference used to save the observation
 * @param jsonStr The observation (string or json)
 * @param agentId The id of the agent which received this observation
 */
void ObservationFromJSON(Observation& obs, const nlohmann::json& json, int agentId);

/**
 * @brief ObservationFromJSON Converts an observation string from python to an observation.
 * @param json The observation (string or json)
 * @param agentId The id of the agent which received this observation
 * @return The observation created from the string
 */
Observation ObservationFromJSON(const nlohmann::json& json, int agentId);

#endif // FROM_JSON_H
