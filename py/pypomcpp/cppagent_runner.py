import logging
import os
from distutils.util import strtobool

from pommerman.runner import DockerAgentRunner
from pypomcpp.cppagent import CppAgent

import argparse


class CppAgentRunner(DockerAgentRunner):
    """
    A docker agent runner for agents implemented via CppAgent.
    """

    def __init__(self, **kwargs):
        super().__init__()
        # create agent
        self._agent = CppAgent(**kwargs)
        # set agent functions (except for act because it is marked abstract)
        self.init_agent = self._agent.init_agent
        self.episode_end = self._agent.episode_end
        self.shutdown = self._agent.shutdown

    def act(self, observation, action_space):
        return self._agent.act(observation, action_space)


def os_env_or_default(key, default):
    """
    Checks if there is an environment variable with the given key. If yes, returns its value. Otherwise, return the
    given default value.

    :param key: The key of the environment variable
    :param default: The default value
    :return: value of the environment variable iff it exists, default otherwise
    """
    env_val = os.environ.get(key)
    return default if env_val is None else env_val


def main():
    """
    Creates and starts the CppAgentRunner.
    """
    parser = argparse.ArgumentParser(description='Start a docker agent runner with a CppAgent.',
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('--agent-name', dest="agent_name", type=str,
                        default=os_env_or_default('agent_name', 'SimpleAgent'),
                        help='name of the cpp agent')

    parser.add_argument('--library-path', dest='library_path', type=str,
                        default=os_env_or_default('library_path', './libpomcpp.so'),
                        help='path of the pomcpp library')

    parser.add_argument('--seed', dest='seed', type=int,
                        default=int(os_env_or_default('seed', "42")),
                        help='seed for the agent')

    parser.add_argument('--verbose', dest='verbose', type=strtobool,
                        default=os_env_or_default('verbose', "False") == "True",
                        help='print verbose information')

    args = parser.parse_args()

    if args.verbose:
        print("Args:")
        for arg in vars(args):
            print(f"> {arg}={getattr(args, arg)}")
        print('', end='', flush=True)

    # set log level for DockerAgentRunner
    log = logging.getLogger('werkzeug')
    if args.verbose:
        log.setLevel(logging.INFO)
    else:
        log.setLevel(logging.ERROR)

    agent = CppAgentRunner(
        library_path=args.library_path,
        agent_name=args.agent_name,
        seed=args.seed,
        print_json=args.verbose
    )
    agent.run()


if __name__ == "__main__":
    main()
