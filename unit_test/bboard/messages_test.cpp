#include <iostream>

#include "catch.hpp"
#include "bboard.hpp"

#include "testing_utilities.hpp"

using namespace bboard;

TEST_CASE("Python Environment Messages", "[message]")
{
    SECTION("Valid Messages")
    {
        PythonEnvMessage msg(1, 3);
        REQUIRE(msg.IsValid());

        PythonEnvMessage msg2(7, 7);
        REQUIRE(msg2.IsValid());

        PythonEnvMessage msg3(0, 0);
        REQUIRE(msg3.IsValid());

        PythonEnvMessage msg4(4, 4);
        REQUIRE(msg4.IsValid());
    }
    SECTION("Invalid Messages")
    {
        PythonEnvMessage msg(8, 2);
        REQUIRE(!msg.IsValid());

        PythonEnvMessage msg2(1, 19);
        REQUIRE(!msg2.IsValid());

        PythonEnvMessage msg3(-1, 0);
        REQUIRE(!msg3.IsValid());

        PythonEnvMessage msg4(3, -5);
        REQUIRE(!msg4.IsValid());
    }
}

TEST_CASE("Message Delivery", "[message]")
{
    Environment e;

    std::mt19937 rng(0);
    auto agents = CreateAgents(rng);
    e.MakeGame(ToPointerArray(agents), GameMode::TeamRadio);

    SECTION("Initialization")
    {
        // episode starts with empty inbox and empty outbox
        for(int i = 0; i < bboard::AGENT_COUNT; i++)
        {
            REQUIRE(!agents[i].incoming);
            REQUIRE(!agents[i].outgoing);
        }
    }

    SECTION("Simple Delivery")
    {
        // we send two messages

        agents[0].SendMessage(0, 2);
        REQUIRE(agents[0].outgoing);

        agents[1].SendMessage(1, 3);
        REQUIRE(agents[1].outgoing);

        // the agents receive the messages in the next step
        e.Step(false);

        REQUIRE(!agents[0].outgoing);
        REQUIRE(!agents[1].outgoing);

        REQUIRE(agents[2].incoming);
        REQUIRE(agents[3].incoming);

        PythonEnvMessage* msg0 = agents[2].TryReadMessage();
        REQUIRE(msg0 != nullptr);
        REQUIRE(msg0->words[0] == 0);
        REQUIRE(msg0->words[1] == 2);

        PythonEnvMessage* msg1 = agents[3].TryReadMessage();
        REQUIRE(msg1 != nullptr);
        REQUIRE(msg1->words[0] == 1);
        REQUIRE(msg1->words[1] == 3);

        // incoming messages are automatically cleared in the next step
        e.Step(false);
        REQUIRE(!agents[2].incoming);
        REQUIRE(!agents[3].incoming);
    }
}
