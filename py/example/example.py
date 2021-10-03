import socketserver
import pommerman
from pommerman.agents import DockerAgent, HttpAgent, SimpleAgent

from pypomcpp import CppAgent, AutoCopy
from util import evaluate, get_free_port


def create_agent_list(setting: str, autolib: AutoCopy, verbose: bool = False):
    """
    Creates a mix of Python and pomcpp agents.

    :param setting: String that specifies the setting
    :param autolib: AutoCopy instance that returns unique library paths
    :param verbose: Print verbose information
    :return: a list of agents
    """
    if setting == 'py':
        return [
            SimpleAgent(),
            SimpleAgent(),
            SimpleAgent(),
            SimpleAgent(),
        ]
    if setting == 'py_pomcpp':
        return [
            SimpleAgent(),
            SimpleAgent(),
            CppAgent(autolib(), "SimpleAgent", seed=14, print_json=verbose),
            CppAgent(autolib(), "SimpleAgent", seed=15, print_json=verbose),
        ]
    if setting == 'pomcpp':
        return [
            CppAgent(autolib(), "SimpleAgent", seed=14, print_json=verbose),
            CppAgent(autolib(), "SimpleAgent", seed=15, print_json=verbose),
            CppAgent(autolib(), "SimpleAgent", seed=16, print_json=verbose),
            CppAgent(autolib(), "SimpleAgent", seed=17, print_json=verbose),
        ]
    if setting == 'py_docker':
        return [
            SimpleAgent(),
            SimpleAgent(),
            SimpleAgent(),
            # run docker image (and bind it to a free port)
            DockerAgent("pomcpp2", get_free_port(), env_vars=dict(
                agent_name="SimpleAgent",
                verbose=verbose
            )),
        ]
    if setting == 'py_http':
        return [
            SimpleAgent(),
            SimpleAgent(),
            SimpleAgent(),
            # debug HttpAgent without rebuilding your image
            HttpAgent(port=10080),
        ]

    raise ValueError(f"Unknown setting '{setting}'.")


def main():
    # Automatically copy the library to allow instantiating multiple cpp agents
    autolib = AutoCopy("./libpomcpp.so", "./libpomcpp_tmp")

    # Create a list of agents
    agent_list = create_agent_list('py_pomcpp', autolib)

    # Create an environment for the agents
    # env = pommerman.make('PommeFFACompetition-v0', agent_list)
    # env = pommerman.make('PommeTeam-v0', agent_list)
    env = pommerman.make('PommeRadioCompetition-v2', agent_list)

    # Optional: Allow CppAgents to cheat (expose whole board)
    use_env_state = False
    if use_env_state:
        CppAgent.use_env_state_list(agent_list, env)

    # Run the game
    evaluate(env, 10, True, False)
    autolib.delete_copies()


if __name__ == "__main__":
    main()
