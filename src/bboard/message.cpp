#include "bboard.hpp"
#include "nlohmann/json.hpp"

namespace bboard
{

Message::Message(int sender, int receiver) : sender(sender), receiver(receiver) {}

bool _checkValid(const PythonEnvMessage* msg)
{
    return msg->content[0] >= 0 && msg->content[0] <= 7 && msg->content[1] >= 0 && msg->content[1] <= 7;
}

PythonEnvMessage::PythonEnvMessage(int sender, int receiver, std::array<int, 2> content)
: Message(sender, receiver), content(content)
{
    valid = _checkValid(this);
}

std::unique_ptr<PythonEnvMessage> PythonEnvMessage::FromJSON(std::string json)
{
    auto parsed = nlohmann::json::parse(json);
    if (parsed.is_discarded()) {
        // received incorrect json
        return std::unique_ptr<PythonEnvMessage>(nullptr);
    }

    int sender = parsed["sender"].get<int>();
    int receiver = parsed["receiver"].get<int>();

    std::array<int, 2> content;
    content[0] = parsed["content"][0].get<int>();
    content[1] = parsed["content"][1].get<int>();

    return std::make_unique<PythonEnvMessage>(sender, receiver, content);
}

std::string PythonEnvMessage::ToJSON()
{
    nlohmann::json json;
    json["sender"] = sender;
    json["receiver"] = receiver;
    json["content"] = content;
    return json.dump();
}

bool PythonEnvMessage::IsValid()
{
    return valid;
}

std::ostream& operator<<(std::ostream &ostream, const PythonEnvMessage &msg) {
    return ostream << "PythonEnvMessage(" << msg.sender << "->" << msg.receiver << ": [" << msg.content[0] << ", " << msg.content[1] << "])";
}

}
