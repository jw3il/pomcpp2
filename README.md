# Pommerman C++ Environment (v2)

[![Build Badge](https://github.com/jw3il/pomcpp2/workflows/build/badge.svg)](https://github.com/jw3il/pomcpp2/actions?query=workflow%3Abuild) 
[![Test badge](https://github.com/jw3il/pomcpp2/workflows/test/badge.svg)](https://github.com/jw3il/pomcpp2/actions?query=workflow%3Atest)

*Disclaimer: This project started as a fork of [pomcpp](https://github.com/m2q/pomcpp) by Adrian Alic. It fixes some bugs and provides new features.*

This repository is an open-source re-implementation of the [Pommerman](https://www.pommerman.com/) multi-agent RL environment. Its aim is to provide a blazing fast alternative to the current python backend - ideally to make computationally expensive methods like tree search feasible. The simulation has (almost) no heap allocations. This is how the C++ side currently looks like.

![gif of the game](docs/gifs/08_08.gif)

## New and Planned Features
- [x] Partial Observability
- [x] Team Mode
- [x] Better consistency with the Python environment
- [x] Python Interface for C++ Agent Implementations
- [x] Improved SimpleAgents
- [x] GitHub Actions
- [x] Communication

## Prerequisites

To compile and run this project from source you will require

- Linux Distribution (tested on Ubuntu 21.04)
- GCC (tested with 10.3.0)
- CMAKE (tested with 3.18.4)
- MAKE (tested with 4.3)

## Setup

#### Download

To clone the repository use

```
$ git clone https://github.com/jw3il/pomcpp2
```

#### Compilation

* Use `./run.sh` to **compile** and **run** the main application.
* Use `./test.sh` to **compile** and **test** the project. We use the [Catch2 Unit Testing Framework](https://github.com/catchorg/Catch2).
* Use `./build.sh` to build everything.

Instead of using the shell scripts you can obviously use make commands and call/debug the binaries yourself. Take a look at the `CMakeLists.txt` for the available targets.

## Build as shared library

Building the project with `make pomcpp_lib` creates a shared library called `libpomcpp.so`. This contains the `bboard` and `agents` namespace. Include the headers in `./include/*` and you're good to go.

This library can also be used to include you c++ agents in Python and to build docker images. More details can be found in the python interface section.

## Project Structure

All of the main source code is in `src/*` and all testing code is in `unit_test/*`. The source is divided into modules

```
src
 |
 |_ _ _ bboard
 |        |_ _ _ bboard.hpp
 |        |_ _ _ ..
 |
 |_ _ _ agents
 |        |_ _ _ agents.hpp
 |        |_ _ _ ..
 |
 |_ _ _ main.cpp
```

All environment specific functions (forward, board init, board masking etc) reside in `bboard`. Agents can be declared
in the `agents` header and implemented in the same module.

All test cases will be in the module `unit_test`. The bboard should be tested thoroughly so it exactly matches the specified behaviour of Pommerman. The compiled `test` binary can be found in `/bin`

## Defining Agents

To create a new agent you can use the base struct defined in `bboard.hpp`. To add your own agent, declare it in
`agents/agents.hpp` and provide a source file in the same module. For example:

agents.hpp (excerpt)
```C++
/**
 * @brief Uses a hand-crafted FSM with stochastic noise
 */
struct MyNewAgent : bboard::Agent
{
    bboard::Move act(const bboard::State* state) override;
};
```

fsm_agent.cpp
```C++
#include "bboard.hpp"
#include "agents.hpp"

namespace agents
{

bboard::Move MyNewAgent::act(const bboard::State* state)
{
    // TODO: Implement your logic
    return bboard::Move::IDLE;
}

}
```

## Python Interface

The python interface consists of two main parts:

* The `PyInterface` namespace (see `include/pymethods.hpp`) allows to control objects of type `bboard::Agent` externally.  
* The `pypomcpp` python package (see `py/pypomcpp`) calls the functions defined in `PyInterface` via `ctypes`.

#### Prerequisites

Install the `pypomcpp` package

```
pip install py
```

#### How to use the interface

1. Optional: Add your own agent to `src/pyinterface/new_agent.cpp`
    ```C++
    bboard::Agent* PyInterface::new_agent(std::string agentName, long seed)
    {
        ...
        else if(agentName == "MyNewAgent")
        {
            return new agents::MyNewAgent();
        }
        ...
    }
    ```
2. Build `pomcpp` as a shared library (including your agent)
4. Load your agent in Python using `pypomcpp`
    ```Python
    from pypomcpp import CppAgent
    my_agent = CppAgent('libpomcpp.so', 'MyNewAgent')
    ```

Note that you have to create copies of your shared library if you want to instantiate multiple agents.
This can be done automatically with `AutoCopy`, you can find an example in `py/example/example.py`.

## Docker

You can build Docker images to run your custom agents on other systems without having to worry about dependency management.

The `pommerman` package already contains the functionality to run agents via Docker containers.
You can use `pypomcpp/cppagent_runner.py` as an entry point in your Dockerfile to listen for instructions from `pommerman`.

An example Dockerfile is included in this repository.

You can build the image

```
docker build -t pomcpp2:latest . 
```

And verify that is runs

```
docker run --rm --name pomcpp2_container pomcpp2 --agent-name MyNewAgent --verbose True
```

If everything looks okay, you can use your image in Python

```Python
DockerAgent("pomcpp2", free_port(), env_vars=dict(
  agent_name="MyNewAgent",
  verbose=False
))
```

Note that you can directly pass arguments to your agent's docker container using the `env_vars`.

## Testing

Want to test out how many steps can be simulated on your machine in 100ms?

```
# example of 4 threads on an Intel i5 (Skylake/4 cores)
$ ./performance.sh -t 4 

Activated Multi-Threading for performance tests. 
	Thread count:            4
	Max supported threads:   4

Test Results:
Iteration count (100ms):         586.332
Tested with:                     agents::HarmlessAgent

===============================================================================
All tests passed (1 assertion in 1 test case)

```
You can also directly run the test-binaries. For a list of command line arguments
see the Catch2 CLI docs (or run `./test --help`). Here are some typical examples
I use a lot:

| Command | What it does |
| ------- | ------------ |
| `./pomcpp_test`  | Runs all tests, including a performance report |
| `./pomcpp_test "[step function]"` | Tests only the step function  |
| `./pomcpp_test ~"[agent statistics]" ~"[performance]"` | Runs all test except the performance and agent stat cases |

## Contributors

The [pomcpp](https://github.com/m2q/pomcpp) project was originally created by [Adrian Alic (m2q)](https://github.com/m2q).
This repository builds upon their work, fixes some bugs and provides new features.

Special thanks to [Márton Görög (gorogm)](https://github.com/gorogm) for providing insights, bug fixes and crucial test cases.

Many thanks to [Johannes Czech (QueensGambit)](https://github.com/QueensGambit) for providing valuable suggestions regarding new features, bug fixes and the usability of the environment.

## Citing This Repo

```
@misc{weil2021pomcpp2,
  author = {Weil, Jannis and Alic, Adrian},
  title = {pomcpp2: Pommerman Environment in {C++}},
  year = {2021},
  publisher = {GitHub},
  journal = {GitHub repository},
  howpublished = {\url{https://github.com/jw3il/pomcpp2}}
}

```
