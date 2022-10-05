import socketserver

import pommerman
from pommerman import utility
from pommerman.constants import GameType
from pommerman.envs.v0 import Pomme
import time
import numpy as np
from pypomcpp.util_plotting import EvalPlotter


def evaluate(env: Pomme, episodes, verbose, visualize, stop=False):
    """
    Evaluates the given pommerman environment (already includes the agents).

    :param episodes: The number of episodes
    :param verbose: Whether to print verbose status information
    :param visualize: Whether to visualize the execution
    :param stop: Whether to wait for input after each step
    :return: The results of the evaluation of shape (episodes, 5) where the first column [:, 0] contains the result
             of the match (tie, win, incomplete) and the remaining columns contain the individual (final) rewards.
    """

    # first element: result, additional elements: rewards
    steps = np.empty(episodes)
    results = np.empty((episodes, 1 + 4))

    start = time.time()

    plotter = EvalPlotter(env)
    # Run the episodes just like OpenAI Gym
    for i_episode in range(episodes):
        state = env.reset()
        plotter.step(state, i_episode)
        done = False
        reward = []
        info = {}
        step = 0
        while not done:
            if visualize:
                env.render()
            actions = env.act(state)
            state, reward, done, info = env.step(actions)
            plotter.step(state, i_episode)
            step += 1

            if False and verbose and step % 10 == 0:
                delta = time.time() - start
                print('\r{:.2f} sec > Episode {} running.. step {}'.format(
                    delta, i_episode, step
                ), end='')

            if stop:
                input()

        steps[i_episode] = step

        result = info['result']
        # save the result
        results[i_episode, 0] = result.value
        results[i_episode, 1:] = reward

        if verbose:
            delta = time.time() - start
            print('\r{:.2f} sec > Episode {} finished with {} ({})'.format(
                delta, i_episode, result, reward
            ))

            if i_episode % 10 == 9 and i_episode != episodes - 1:
                print_stats(env, results, steps, i_episode + 1)

    env.close()

    if verbose:
        delta = time.time() - start
        print("Total time: {:.2f} sec".format(delta))
        print_stats(env, results, steps, episodes)


    # plot histogram of visited states

    plotter.plot_explored_positions(agent_ixs=[0])
    plotter.plot_state_varieties(agent_ixs=[0], modes=["aggregated", "bars", "grid", "stacked", "timeseries"])
    plotter.plot_placed_bombs([0])
    plotter.plot_placed_bombs([0,1,2,3])


    return results


def print_stats(env, results, steps, episodes):
    if env._game_type == GameType.FFA:
        ffa_print_stats(results, steps, episodes)
        print("Opposite positions {}".format([x/sum(env.opposite_position_counter) for x in env.opposite_position_counter]))
    elif env._game_type == GameType.Team or env._game_type == GameType.TeamRadio:
        team_print_stats(results, steps, episodes)


def team_print_stats(results, steps, episodes):
    num_won, num_ties = get_stats(results, episodes)
    assert num_won[0] == num_won[2]
    assert num_won[1] == num_won[3]

    print("Evaluated {} episodes".format(episodes))
    print("Average steps: {}".format(steps[:episodes].mean()))

    total_won = int(np.sum(num_won) / 2)
    print("Wins: {} ({:.2f}%)".format(total_won, total_won / episodes * 100))
    print("> Team 0 (Agent 0, 2): {} ({:.2f}%)".format(
        num_won[0], 0 if total_won == 0 else num_won[0] / total_won * 100))
    print("> Team 1 (Agent 1, 3): {} ({:.2f}%)".format(
        num_won[1], 0 if total_won == 0 else num_won[1] / total_won * 100))

    print("Ties: {} ({:.2f}%)".format(num_ties, num_ties / episodes * 100))

    assert np.sum(num_won) / 2 + num_ties == episodes


def ffa_print_stats(results, steps, episodes):
    num_won, num_ties = get_stats(results, episodes)

    print("Evaluated {} episodes".format(episodes))
    print("Average steps: {}".format(steps[:episodes].mean()))
    print("Median steps: {}".format(np.median(steps[:episodes])))

    total_won = np.sum(num_won)
    print("Wins: {} ({:.2f}%)".format(total_won, total_won / episodes * 100))
    for a in range(len(num_won)):
        print("> Agent {}: {} ({:.2f}%)".format(a, num_won[a], num_won[a] / total_won * 100))

    print("Ties: {} ({:.2f}%)".format(num_ties, num_ties / episodes * 100))

    assert np.sum(num_won) + num_ties == episodes


def get_stats(results, episodes):
    # Count how often each agent achieved a final reward of "1"
    num_won = np.sum(results[0:episodes, 1:] == 1, axis=0)
    # In a tie, every player receives -1 reward
    num_ties = np.sum(results[0:episodes, 0] == pommerman.constants.Result.Tie.value)

    return num_won, num_ties


def get_free_port():
    """
    Get a random free port.

    :return: a free port.
    """

    # noinspection PyTypeChecker
    # see https://stackoverflow.com/questions/1365265/on-localhost-how-do-i-pick-a-free-port-number
    with socketserver.TCPServer(("localhost", 0), None) as s:
        return s.server_address[1]
