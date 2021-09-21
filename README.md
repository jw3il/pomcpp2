# Pommerman C++ Environment (v2)

[![Build Badge](https://github.com/jw3il/pomcpp2/workflows/build/badge.svg)](https://github.com/jw3il/pomcpp2/actions?query=workflow%3Abuild) 
[![Test badge](https://github.com/jw3il/pomcpp2/workflows/test/badge.svg)](https://github.com/jw3il/pomcpp2/actions?query=workflow%3Atest)

*Disclaimer: This project started as a fork of [pomcpp](https://github.com/m2q/pomcpp) by Adrian Alic. It fixes some bugs and provides new features.*

This repository is an open-source re-implementation of the [Pommerman](https://www.pommerman.com/) multi-agent RL environment. Its aim is to provide a blazing fast and alternative to the current python backend - ideally to make computationally expensive methods like tree search feasible. The simulation has no heap allocations. This is how the C++ side currently looks like.

![gif of the game](docs/gifs/08_08.gif)

## New and Planned Features
- [x] Partial Observability
- [x] Team Mode
- [x] Better consistency with the Python environment
- [x] Python Interface for C++ Agent Implementations
- [x] Improved SimpleAgents
- [x] GitHub Actions
- [x] Communication
- [ ] Variable board size (without recompilation)

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

## Use Pommerman C++ as a library

Building the project with `make pomcpp_lib` compiles a shared library called `libpomcpp.so`. This contains the `bboard` and `agents` namespace. Include the headers in `./include/*` and you're good to go.

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

## Defining Agents

To create a new agent you can use the base struct defined in `bboard.hpp`. To add your own agent, declare it in
`agents/agents.hpp`and provide a source file in the same module. For example:

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

## Contributors

Special thanks to [Márton Görög (gorogm)](https://github.com/gorogm) for providing insights, bug fixes and crucial test cases.


## Citing This Repo

```
@misc{Alic2018,
  author = {Alic, Adrian and Weil, Jannis},
  title = {Pommerman C++ Implementation},
  year = {2021},
  publisher = {GitHub},
  journal = {GitHub repository},
  howpublished = {\url{https://github.com/jw3il/pomcpp2}}
}

```
