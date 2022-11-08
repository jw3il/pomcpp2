
from itertools import zip_longest
import math
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from pommerman.agents.docker_agent import DockerAgent
import pommerman.constants as constants
from pommerman.envs.v0 import Pomme
from typing import List
import os
from pypomcpp.cppagent import CppAgent

FIG_WIDTH = 8

def _agent_name(agent):
    if isinstance(agent, CppAgent):
        name = f"cpp_{agent.agent_name.split('Agent')[0]}"
    elif isinstance(agent, DockerAgent):
        if "/" in agent._docker_image:
            name = agent._docker_image.split('/')[-1]
        else:
            name = agent._docker_image
    else:
        name = type(agent).__name__
    return name

class EvalPlotter():
    """
    Plotter for the evaluation of a pommerman run.
    At each evaluation step the state is passed to the plotter.
    The plotter is able to create different plots based on the passed steps.
    (see documentation of the plot_... functions)
    """
    def __init__(self, env: Pomme, n_episodes=None, experiment_path=None) -> None:
        """_summary_

        :param env: Pommerman environment used for evaluation loop
        :param n_episodes: number of evaluation episodes (used for naming), defaults to None
        :param experiment_path: path to expiriment folder, if not set plots are saved under ./eval_plotting/"experiment name"
        """
        self.env = env #get #agents, angent_names
        self.n_agents = len(self.env._agents)
        self.episodes = [[]] # list of episodes (a list of states)
        self.current_episode = 0
        self.visited_positions = None
        self.agent_colors = ['tab:blue', 'tab:orange', 'black', 'tab:red']
        self.agent_labels = [_agent_name(agent) + f" - id:{agent._character.agent_id}" for agent in self.env._agents]
        if experiment_path is None:
            self.experiment_path = self._default_dir(n_episodes)
        else:
            self.experiment_path = experiment_path

        if not os.path.exists(self.experiment_path):
            os.mkdir(self.experiment_path)
    
    def _default_dir(self, n_episodes):
        """ creates a default directory if no path was passed to the plotter

        :param n_episodes: number of episodes (used for naming the folder)
        :return: pathlike name of the default directory for the results
        """
        suffix = f"_e_{n_episodes}" if n_episodes is not None else "" 
        experiment_name = f"{type(self.env).__name__}{suffix}"
        if not os.path.exists("./eval_plotting"):
            os.mkdir("./eval_plotting")
        i = 0
        while os.path.exists(f"./eval_plotting/{experiment_name}_{i}"):
            i += 1

        eval_dir=f"./eval_plotting/{experiment_name}_{i}"
        
        return eval_dir

    def step(self, state, episode):
        """function to pass the current state to the plotter during evaluation

        :param state: current environment state
        :param episode: current evaluation episode
        """
        if episode != self.current_episode:
            self.episodes.append([])
            self.current_episode = episode
        self.episodes[-1].append(state)

    def _agent_alive(self, agent_state):
        """

        :param agent_state: state of an agent enf.state[agend_ix]
        :return: wether the agent is alive at the passed state
        """
        n_alive = len(agent_state["alive"])
        others = [en.value for en in agent_state["enemies"]] + [agent_state['teammate'].value]
        n_others_alive = len(set(agent_state["alive"]) & set(others))
        return n_alive-n_others_alive

    def _plot_attribute(self, agent_ixs, get_attribute, y_label, fig_name, individual_plots=False, plot_agents_alive=False):
        """_summary_

        :param agent_ixs: indicies of the agents that should be plotted
        :param get_attribute: function that gathers the desired attribute with input (positions[e_nr], episode, agent_ix) 
        :param y_label: y-label of the plot
        :param fig_name: plot name
        :param individual_plots: create additional plots for each individual agent, defaults to False
        :param plot_agents_alive: add a dashed line showing how many agents where alive/used for the aggregated result, defaults to False
        """
        # agent labels
        labels = [self.agent_labels[i] for i in agent_ixs]
        #individual plots
        if individual_plots:
                ifig, iaxs = plt.subplots(1, len(agent_ixs), sharex=True, sharey=True, figsize=(16,4))

        # aggregated plot
        fig, ax1 = plt.subplots(sharex=True, sharey=True)
        fig.set_figwidth(FIG_WIDTH)
        if plot_agents_alive:
            ax2 = ax1.twinx()
            ax2.set_ylabel("Agents alive")
        for agent_ix in agent_ixs:
            positions = self._visited_positions()[agent_ix]
            episodes = []
            for e_nr, episode in enumerate(self.episodes):
                episode = get_attribute(positions[e_nr], episode, agent_ix)
                episodes.append(episode)
            
            #individual plots
            if individual_plots:
                for i, e in enumerate(episodes):
                    if len(agent_ixs)>1:
                        iaxs[agent_ix].plot(e, label=f"episode {i}")
                        iaxs[agent_ix].title.set_text(labels[agent_ix])
                    else:
                        iaxs.plot(e, label=f"episode {i}")

            # aggregated plot
            means, n_alive = _aggregate_timeseries(episodes)
            ax1.plot(means, label=y_label, color=self.agent_colors[agent_ix])
            if plot_agents_alive:
                ax2.plot(n_alive, label="# agents alive", color=self.agent_colors[agent_ix], linestyle="dashed")
        
        ax1.set_ylabel(y_label)
        ax1.set_xlabel("Steps")
        patches = [mpatches.Patch(color=self.agent_colors[ix], label=labels[ix]) for ix in agent_ixs]
        ax1.legend(handles=patches, loc='upper center', bbox_to_anchor=(0.5, -.15), fancybox=True, shadow=True, ncol=len(agent_ixs))
        fig.tight_layout()
        fig.savefig(os.path.join(self.experiment_path, f"{fig_name}_aggregated"), bbox_inches="tight")

        if individual_plots:
            if len(agent_ixs)>1:
                ax = iaxs[agent_ixs[0]]
            else:
                ax = iaxs
            fig.text(0.5, 0.04, 'Steps', ha='center')
            ax.set_ylabel(y_label)
            ifig.savefig(os.path.join(self.experiment_path, f"{fig_name}_individual"), bbox_inches="tight")

    def plot_kicks(self, agent_ixs, individual_plots=False, plot_agents_alive=False):
        """Creates a plot showing the number of kicks for each agent

        :param agent_ixs: indicies of the agents that should be plotted
        :param individual_plots: create additional plots for each individual agent, defaults to False
        :param plot_agents_alive: add a dashed line showing how many agents where alive/used for the aggregated result, defaults to False
        """
        def _kicks_episode(positions, episode, agent_ix):
            kicks = [0]
            for (previous_state, current_position) in zip(episode[:len(positions)+1], positions[1:]):
                # The edge case where agent kicks a bomb which is moving towards him is ignored
                bomb_at_dest = previous_state[agent_ix]["bomb_life"][current_position]>0 and previous_state[agent_ix]["bomb_moving_direction"][current_position]==0
                moved = previous_state[agent_ix]["position"] != current_position
                kick = bomb_at_dest and moved and previous_state[agent_ix]["can_kick"]
                kicks.append(kicks[-1] + kick)
            return kicks
        self._plot_attribute(agent_ixs, get_attribute=_kicks_episode, y_label="kicks", fig_name="kicks", individual_plots=individual_plots, plot_agents_alive=plot_agents_alive)
      
    def plot_collected_powerups(self, agent_ixs, individual_plots=False, plot_agents_alive=False):
        """Creates a plot showing the number of collected powerups for each agent

        :param agent_ixs: indicies of the agents that should be plotted
        :param individual_plots: create additional plots for each individual agent, defaults to False
        :param plot_agents_alive: add a dashed line showing how many agents where alive/used for the aggregated result, defaults to False
        """
        def _collected_item_episode(positions, episode, agent_ix):
            collected_items = [0]
            for (state, next_position) in zip(episode[:len(positions)+1], positions[1:]):
                collected = state[agent_ix]["board"][next_position] in [constants.Item.ExtraBomb.value, constants.Item.IncrRange.value, constants.Item.Kick.value]
                collected_items.append(collected_items[-1] + collected)
            return collected_items

        self._plot_attribute(agent_ixs, get_attribute=_collected_item_episode, y_label="powerups collected", fig_name="collected_powerups", individual_plots=individual_plots, plot_agents_alive=plot_agents_alive)

    def plot_placed_bombs(self, agent_ixs, individual_plots=False, plot_agents_alive=False):
        """Creates a plot showing the number of placed bombs for each agent

        :param agent_ixs: indicies of the agents that should be plotted
        :param individual_plots: create additional plots for each individual agent, defaults to False
        :param plot_agents_alive: add a dashed line showing how many agents where alive/used for the aggregated result, defaults to False
        """
        def _placed_bomb_episode(positions, episode, agent_ix):
            placed_bombs = [0]
            for (state, previous_position) in zip(episode[1:len(positions)], positions[:-1]):
                placed_bomb = state[agent_ix]["bomb_life"][previous_position]==9
                placed_bombs.append(placed_bombs[-1] + placed_bomb)
            return placed_bombs

        self._plot_attribute(agent_ixs, get_attribute=_placed_bomb_episode, y_label="bombs placed", fig_name="placed_bombs", individual_plots=individual_plots, plot_agents_alive=plot_agents_alive)

    def _visited_positions(self):
        """
        returns a 3 dimensional list of visited positions for the agents with the following structure:
        visited_positions[agent][episode][step]
        """
        def _get_visited_positions():
            visited_positions = [[] for _ in range(self.n_agents)] 
            for episode in self.episodes:
                visited = [[] for _ in range(self.n_agents)] 
                for states in episode: 
                    for ix, state in enumerate(states):
                        if self._agent_alive(state):
                            visited[ix].append(state["position"])
                for agent_ix in range(self.n_agents):
                    visited_positions[agent_ix].append(visited[agent_ix])
            return visited_positions

        if self.visited_positions is None:
            self.visited_positions = _get_visited_positions()
        return self.visited_positions

    def plot_explored_positions_per_agent(self, agent_ixs):
        """Creates a plot of the number of unique positions at each step per agent

        :param agent_ixs: indicies of agents to be plotted
        """
        if agent_ixs is None:
            agent_ixs = list(range(self.n_agents))
        # plot single episodes
        fig, ax = plt.subplots()
        for agent_ix in agent_ixs:
            visited_positions = self._visited_positions()[agent_ix]
            unique_ixs = [np.sort(np.unique(positions, return_index=True, axis=0)[1]) for positions in visited_positions]
            unique_states = [list(range(1,len(ixs)+1)) for ixs in unique_ixs]

            for (x,y) in zip(unique_ixs, unique_states):
                ax.plot(x,y, color=self.agent_colors[agent_ix])
        
        labels = [self.agent_labels[i] for i in agent_ixs]
        patches = [mpatches.Patch(color=self.agent_colors[ix], label=labels[ix]) for ix in agent_ixs]
        ax.legend(handles=patches, loc='upper center', bbox_to_anchor=(0.5, -.15), fancybox=True, shadow=True, ncol=len(agent_ixs))
        ax.set_ylabel('unique states visited')
        ax.set_xlabel('steps')

        fig.savefig(os.path.join(self.experiment_path,"visited unique positions"), bbox_inches="tight")
        plt.clf()

    def plot_explored_positions(self, agent_ixs=None, plot_agents_alive=True):
        """Creates an aggregated plot of "plot_explored_positions_per_agent" aggregating over the agent id

        :param agent_ixs: indicies of agents to be plotted
        :param plot_agents_alive: plot how many agents were alive (thereby accounted for during averaging), defaults to True
        """
        if agent_ixs is None:
            agent_ixs = list(range(self.n_agents))

        fig, ax1 = plt.subplots(constrained_layout=True)
        fig.set_figwidth(FIG_WIDTH)
        if plot_agents_alive:
            ax2 = ax1.twinx()
            ax2.set_ylabel('agents alive')

        labels = [self.agent_labels[i] for i in agent_ixs]
        patches = [mpatches.Patch(color=self.agent_colors[ix], label=labels[ix]) for ix in agent_ixs]

        for agent_ix in agent_ixs:
            # plot visited positions per agent
            visited_positions = self._visited_positions()[agent_ix]
            unique_ixs = [np.sort(np.unique(positions, return_index=True, axis=0)[1]) for positions in visited_positions]
            
            unique_positions = [] 
            for i, ixs in enumerate(unique_ixs):
                unique_positions.append([len(ixs[ixs<=ix]) for ix in range(0,len(visited_positions[i]))])

            means, n_alive = _aggregate_timeseries(unique_positions)
            
            ax1.plot(means, color=self.agent_colors[agent_ix], label="unique states" + labels[agent_ix])
            if plot_agents_alive:
                ax2.plot(n_alive, color=self.agent_colors[agent_ix], linestyle='dashed', label="alive" + labels[agent_ix])
        
        ax1.set_ylabel('unique states visited')

        if len(agent_ixs)>1:
            ax1.legend(handles=patches, loc='upper center', bbox_to_anchor=(0.5, -.15), fancybox=True, shadow=True, ncol=len(agent_ixs))

        ax1.set_xlabel(f"steps")
        fig.savefig(os.path.join(self.experiment_path,f"visited unique positions_aggregated"), bbox_inches="tight")
        plt.clf()

    def plot_state_varieties(self, agent_ixs=None, max_variety=20, modes=None, plot_agents_alive=True):
        """Creates a plot displaying how many uniques states the agent has visited within the last x moves at each step

        :param agent_ixs: list of agent ids that should be included in the plot, defaults to all agents
        :param max_variety:  size of sliding window which is used to count unique visited states. defaults to 20
        :param modes: list of modes for plot generation: "aggregated", "timeseries", "bars"(only good for small amount of episodes <=~5), defaults to None
        """

        if agent_ixs is None:
            agent_ixs = list(range(self.n_agents))

        # structure state varieties in the following way:
        # state_varieties[agent, episode, step]
        state_varieties = []
        for agent_ix in agent_ixs:
            agent_state_varieties = []
            for episode in self.episodes:
                positions = [state[agent_ix]["position"] for state in episode if self._agent_alive(state[agent_ix])]
                svs = []
                for i in range(max_variety, len(positions)):
                    window = positions[i-max_variety: i]
                    svs.append(len(set(window)))
                agent_state_varieties.append(svs)
            state_varieties.append(agent_state_varieties)
        
        # plotting
        labels = [self.agent_labels[i] for i in agent_ixs]
        patches = [mpatches.Patch(color=self.agent_colors[ix], label=labels[ix]) for ix in agent_ixs]
        
        for mode in modes:
            if mode in ["timeseries"]: # stacked
                fig, ax = plt.subplots(sharex=True)
                fig.set_figwidth(FIG_WIDTH)
                if plot_agents_alive:
                    ax2 = ax.twinx()
                else:
                    ax2=None
                for a_ix in agent_ixs:
                    self._plot_state_varieties((ax,ax2), state_varieties[a_ix], 20, mode=mode, color=self.agent_colors[a_ix], plot_agents_alive=plot_agents_alive)
                plt.xlabel(f"steps")
                ax.legend(handles=patches, loc='upper center', bbox_to_anchor=(0.5, -.15), fancybox=True, shadow=True, ncol=len(agent_ixs))
            else:
                _, axs = plt.subplots(len(agent_ixs), sharex=True, sharey=True)
        
                for a_ix in agent_ixs:
                    if type(axs)==np.ndarray or (axs.numCols*axs.numRows) > 1:
                        self._plot_state_varieties(axs[a_ix], state_varieties[a_ix], 20, mode=mode, color=self.agent_colors[a_ix], plot_agents_alive=plot_agents_alive)
                        ax = axs[0]
                    else:
                        ax = axs
                        self._plot_state_varieties(ax, state_varieties[a_ix], 20, mode=mode, color=self.agent_colors[a_ix], plot_agents_alive=plot_agents_alive)
                plt.xlabel(f"unique visited stated within {max_variety} moves")
                ax.legend(handles=patches, loc='upper left', bbox_to_anchor=(1, 1.05), fancybox=True, shadow=True)
            plt.tight_layout()
            plt.savefig(os.path.join(self.experiment_path,f"state_variety_window{max_variety}_episodes{len(self.episodes)}_{mode}"), bbox_inches="tight")
            plt.clf()

    def _plot_state_varieties(self, ax, state_varieties: List[List[int]], max_variety, plot_agents_alive, mode=None, color="blue"):
        """helper functio for plot_state_varieties

        :param state_varieties: list of unique visted positions within a sliding window per episode
        :param max_variety: size of sliding window which is used to count unique visited states.
        :param mode: type of plot that should be generated. Possible options: "grid", "aggregated", "stacked", "
        """
        
        if mode is None:
            mode = "aggregated"

        if mode == "bars":
            ax.hist(np.array(state_varieties, dtype=object), bins=max_variety, range=(1, max_variety+1), align="left", histtype='step', fill=False)
            ax.set_xticks(np.arange(1, max_variety+1, 1))
        elif mode == "aggregated":
            ax.hist(np.concatenate(state_varieties), bins=max_variety, range=(1, max_variety+1), align="left", density=True, color=color)
            ax.set_xticks(np.arange(1, max_variety+1, 1))
        elif mode == "timeseries":
            ax1, ax2 = ax
            means, n_alive = _aggregate_timeseries(state_varieties)
            ax1.plot(means, color=color, label="unique states")
            ax1.set_ylabel('unique states', color='black')

            if plot_agents_alive:
                ax2.plot(n_alive, color=color, linestyle='dashed', label="alive")
                ax2.set_ylabel('agents alive', color='black')

    def plot_agents_alive(self, agent_ixs):
        """Plot number of agents alive per id over all episodes

        :param agent_ixs: list of agent ids that should be included in the plot
        """
        # agent labels
        labels = [self.agent_labels[i] for i in agent_ixs]

        # aggregated plot
        fig, ax1 = plt.subplots(sharex=True, sharey=True)
        ax1.set_ylabel("Agents alive")
        for agent_ix in agent_ixs:
            episodes = self._visited_positions()[agent_ix]
            survived_steps = np.sort(np.array([len(positions) for positions in episodes]))
            survived_agents = [sum(survived_steps>0)] + [sum(survived_steps>s) for s in survived_steps]
            x = [0]+survived_steps.tolist()        # aggregated plot
            ax1.step(x, survived_agents, label="# agents alive", color=self.agent_colors[agent_ix], where='post', linestyle="dashed")
        
        ax1.set_ylabel("# agents alive")
        ax1.set_xlabel("Steps")
        patches = [mpatches.Patch(color=self.agent_colors[ix], label=labels[ix]) for ix in agent_ixs]
        ax1.legend(handles=patches, loc='upper center', bbox_to_anchor=(0.5, -.15), fancybox=True, shadow=True, ncol=len(agent_ixs))
        fig.tight_layout()
        fig.savefig(os.path.join(self.experiment_path,"agents_alive"), bbox_inches="tight")

def _aggregate_timeseries(to_aggregate):
    """ aggregate timeseries with various by taking the mean of all non nan values

    :param to_aggregate: attribute that should be aggregated, 
                        (list of episodes containing a list of the attribute: to_aggregate[episode][step]=attribute)

    :return: mean & the number of values considered per timestep (mean=[1.33, 3, 4.5, 6], n_points=[3, 3, 2, 1])
    """
    max_len_combined = np.array(list(zip_longest(*to_aggregate)),dtype=float)
    means = np.nanmean(max_len_combined, axis=1)
    n_points = max_len_combined.shape[1] - np.count_nonzero(np.isnan(max_len_combined),axis=1)
    return means, n_points