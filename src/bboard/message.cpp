#include "bboard.hpp"
#include "nlohmann/json.hpp"

namespace bboard
{

bool PythonEnvMessage::IsValid()
{
    return words[0] >= 0 && words[0] <= 7 && words[1] >= 0 && words[1] <= 7;
}

std::ostream& operator<<(std::ostream &ostream, const PythonEnvMessage &msg) {
    return ostream << "PythonEnvMessage(" << msg.words[0] << ", " << msg.words[1] << ")";
}
}
