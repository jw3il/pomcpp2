import pommerman
import pommerman.agents as agents
import pommerman.utility as utility
from gym.spaces import Tuple, Discrete
from pommerman.constants import Action
from pommerman.envs.v0 import Pomme
from pypomcpp.clib import CLib
import time
import ctypes
import json


class CppAgent(agents.BaseAgent):
    """
    Wrapper for pomcpp agents implemented in c++.
    """

    def __init__(self, library_path, agent_name: str, seed: int = 42, print_json=False):
        super().__init__()
        self.agent_name = agent_name
        self.env = None
        self.print_json = print_json
        self.id = None

        # load interface

        lib = CLib(library_path)
        self.agent_create = lib.get_fun("agent_create", [ctypes.c_char_p, ctypes.c_long], ctypes.c_bool)
        self.agent_reset = lib.get_fun("agent_reset", [], ctypes.c_void_p)
        self.agent_act = lib.get_fun("agent_act", [ctypes.c_char_p, ctypes.c_bool], ctypes.c_int)
        self.get_message = lib.get_fun("get_message", [ctypes.c_void_p, ctypes.c_void_p], ctypes.c_void_p)

        # create agent

        if not self.agent_create(agent_name.encode('utf-8'), seed):
            raise ValueError(f"Could not create agent with name {agent_name}!")

        self.total_steps = 0
        self.sum_encode_time = 0.0
        self.sum_agent_act_time = 0.0

    def use_env_state(self, env: Pomme):
        """
        Use the state from the given environment instead of the actual observation when act is called.

        :param env: The environment
        """
        self.env = env

    @staticmethod
    def use_env_state_list(agent_list, env: Pomme):
        """
        Calls use_env_state for each CppAgent in the given list of agents.

        :param agent_list: A list of agents
        :param env: The environment
        """
        for a in agent_list:
            if isinstance(a, CppAgent):
                a.use_env_state(env)

    def get_state_json(self):
        """
        Get the current environment state as json.

        :return: the current environment state as json.
        """
        env: Pomme = self.env

        state = {
            'game_type': env._game_type,
            'board_size': env._board_size,
            'step_count': env._step_count,
            'board': env._board,
            'agents': env._agents,
            'bombs': env._bombs,
            'flames': env._flames,
            'items': [[k, i] for k, i in env._items.items()],
            'intended_actions': env._intended_actions,
        }

        if hasattr(env, '_radio_from_agent'):
            radio_from_agent = {}
            for agentItem in env._radio_from_agent:
                radio_from_agent[agentItem.value] = env._radio_from_agent[agentItem]
            # add all messages
            state['radio_from_agent'] = radio_from_agent

            # simulate radio message observation
            teammate = (self.id + 2) % 4 + 10
            state['teammate'] = teammate
            state['message'] = radio_from_agent[teammate]

        return json.dumps(state, cls=utility.PommermanJSONEncoder)

    def act(self, obs, action_space):
        act_start = time.time()

        if self.env:
            json_input = self.get_state_json()
            if self.print_json:
                json_obs = json.dumps(obs, cls=utility.PommermanJSONEncoder)
                print("State: ", json_input.replace("\"", "\\\""))
                print("Obs: ", json_obs.replace("\"", "\\\""))
        else:
            json_input = json.dumps(obs, cls=utility.PommermanJSONEncoder)
            if self.print_json:
                print("Obs: ", json_input.replace("\"", "\\\""))

        act_encoded = time.time()

        move = self.agent_act(json_input.encode('utf-8'), self.env is not None)
        act_done = time.time()

        diff_encode = (act_encoded - act_start)
        self.sum_encode_time += diff_encode

        diff_act = (act_done - act_encoded)
        self.sum_agent_act_time += diff_act

        self.total_steps += 1

        # print("Python side: Agent wants to do do move ", move, " = ", Action(move))

        if isinstance(action_space, Discrete):
            assert action_space.n == 6
            has_communication = False
        elif isinstance(action_space, int):
            assert action_space == 6
            has_communication = False
        elif isinstance(action_space, Tuple):
            assert (len(action_space.spaces) == 3 and action_space.spaces[0].n == 6
                    and action_space.spaces[1].n == action_space.spaces[2].n == 8)
            has_communication = True
        elif isinstance(action_space, list):
            assert (len(action_space) == 3 and action_space[0] == 6 and action_space[1] == action_space[2] == 8)
            has_communication = True
        else:
            raise ValueError("Unknown action space ", action_space)

        if has_communication:
            return move
        else:
            # default message value is 0
            word_0 = ctypes.c_int(0)
            word_1 = ctypes.c_int(0)
            self.get_message(ctypes.byref(word_0), ctypes.byref(word_1))

            return [move, word_0.value, word_1.value]

    def init_agent(self, id, game_type):
        super().init_agent(id, game_type)

        if game_type != pommerman.constants.GameType.FFA and game_type != pommerman.constants.GameType.Team \
                and game_type != pommerman.constants.GameType.TeamRadio:
            raise ValueError(f"GameType {str(game_type)} is not supported!")

        self.id = id
        self.agent_reset(id)

    def episode_end(self, reward):
        if self.id is None:
            raise ValueError("Episode ended before id has been set!")

        # as there is no "real" reset in the agent interface, we use episode_end
        self.agent_reset(self.id)

    def print_time_stats(self):
        print(f"Response times of agent '{self.agent_name}' over {self.total_steps} steps:")
        print(f"> Average encode time: {self.sum_encode_time / self.total_steps * 1000} ms")
        print(f"> Average act time: {self.sum_agent_act_time / self.total_steps * 1000} ms")
