#include <iostream>

#include "catch.hpp"
#include "bboard.hpp"

#include "testing_utilities.hpp"

using namespace bboard;

TEST_CASE("Python Environment Messages", "[message]")
{
    SECTION("JSON Conversion")
    {
        PythonEnvMessage msg(0, 1, {4, 2});
        REQUIRE(msg.IsValid());

        // get json
        std::string json = msg.ToJSON();

        // convert back to msg
        std::unique_ptr<PythonEnvMessage> msgCopyPtr = PythonEnvMessage::FromJSON(json);
        REQUIRE(msgCopyPtr != nullptr);
        PythonEnvMessage msgCopy = *msgCopyPtr;
        REQUIRE(msgCopy.IsValid());

        REQUIRE(msg.sender == msgCopy.sender);
        REQUIRE(msg.receiver == msgCopy.receiver);
        REQUIRE(msg.content == msgCopy.content);
    }
    SECTION("Invalid Messages")
    {
        PythonEnvMessage msg(0, 1, {8, 2});
        REQUIRE(!msg.IsValid());

        PythonEnvMessage msg2(0, 1, {1, 19});
        REQUIRE(!msg2.IsValid());

        PythonEnvMessage msg3(0, 1, {-1, 0});
        REQUIRE(!msg3.IsValid());

        PythonEnvMessage msg4(0, 1, {3, -5});
        REQUIRE(!msg4.IsValid());
    }
}

TEST_CASE("Message Delivery", "[message]")
{
    Environment e;
    e.SetCommunication(true);

    std::mt19937 rng(0);
    auto agents = CreateAgents(rng);
    e.MakeGame(ToPointerArray(agents), GameMode::FreeForAll);

    SECTION("Initialization")
    {
        // episode starts with empty inbox and empty outbox

        REQUIRE(agents[0].inbox.size() == 0);
        REQUIRE(agents[1].inbox.size() == 0);
        REQUIRE(agents[2].inbox.size() == 0);
        REQUIRE(agents[3].inbox.size() == 0);

        REQUIRE(agents[0].outbox.size() == 0);
        REQUIRE(agents[1].outbox.size() == 0);
        REQUIRE(agents[2].outbox.size() == 0);
        REQUIRE(agents[3].outbox.size() == 0);
    }

    SECTION("Invalid Sender")
    {
        // agent 2 tries to send a message as agent 0
        agents[2].Send(new PythonEnvMessage(0, 1, {4, 4}));
        REQUIRE(agents[2].outbox.size() == 1);

        std::cout << std::endl;
        std::cout << "Invalid Sender Test" << std::endl;
        e.Step(false);
        std::cout << std::endl;

        // => message is dropped
        // => inbox of agent 1 (receiver) should be empty
        REQUIRE(agents[1].inbox.size() == 0);
    }

    SECTION("Simple Delivery")
    {
        // we send two messages

        agents[0].Send(new PythonEnvMessage(0, 1, {0, 0}));
        REQUIRE(agents[0].outbox.size() == 1);

        agents[1].Send(new PythonEnvMessage(1, 0, {1, 1}));
        REQUIRE(agents[1].outbox.size() == 1);

        // the agents receive the messages in the next step
        e.Step(false);

        REQUIRE(agents[0].outbox.size() == 0);
        REQUIRE(agents[1].outbox.size() == 0);

        REQUIRE(agents[0].inbox.size() == 1);
        REQUIRE(agents[1].inbox.size() == 1);

        PythonEnvMessage* msg0 = agents[0].TryRead<PythonEnvMessage>(0);
        REQUIRE(msg0 != nullptr);
        REQUIRE(msg0->content[0] == 1);
        REQUIRE(msg0->content[1] == 1);

        PythonEnvMessage* msg1 = agents[1].TryRead<PythonEnvMessage>(0);
        REQUIRE(msg1 != nullptr);
        REQUIRE(msg1->content[0] == 0);
        REQUIRE(msg1->content[1] == 0);

        // the inbox is cleared in the next step
        e.Step(false);

        REQUIRE(agents[0].inbox.size() == 0);
        REQUIRE(agents[1].inbox.size() == 0);
    }
}
