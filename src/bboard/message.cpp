#include "bboard.hpp"
#include "nlohmann/json.hpp"

namespace bboard
{

bool PythonEnvMessage::IsValid()
{
    return words[0] >= 1 && words[0] <= 8 && words[1] >= 1 && words[1] <= 8;
}

std::ostream& operator<<(std::ostream &ostream, const PythonEnvMessage &msg) {
    return ostream << "PythonEnvMessage(" << msg.words[0] << ", " << msg.words[1] << ")";
}
}
