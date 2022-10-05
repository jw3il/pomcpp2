
from itertools import zip_longest
import math
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import pommerman.constants as constants
from pommerman.envs.v0 import Pomme
from typing import List


class EvalPlotter():
    def __init__(self, env: Pomme) -> None:
        self.env = env #get #agents, angent_names
        self.n_agents = len(self.env._agents)
        self.episodes = [[]] # list of episodes (a list of states)
        self.current_episode = 0
        self.visited_positions = None
        self.agent_colors = ['tab:blue', 'tab:orange', 'black', 'tab:red']
        pass
    
    
    def step(self, states, episode):
        if episode != self.current_episode:
            self.episodes.append([])
            self.current_episode = episode
        self.episodes[-1].append(states)

    def _agent_alive(self, agent_state):
        return len(agent_state["alive"]) - len(set(agent_state["alive"]) & set([en.value for en in agent_state["enemies"]]))

    def plot_collected_powerups(self, agent_ixs, individual_plots=False):
        # agent labels
        labels = [self.env._agents[i].agent_name.split("Agent")[0] + f" - id:{self.env._agents[i]._character.agent_id}" for i in agent_ixs]
        #individual plots
        if individual_plots:
                ifig, iaxs = plt.subplots(1, len(agent_ixs), sharex=True, sharey=True, figsize=(16,4))

        # aggregated plot
        fig, ax1 = plt.subplots(sharex=True, sharey=True)
        ax2 = ax1.twinx()
        for agent_ix in agent_ixs:
            positions = self._visited_positions()[agent_ix]
            placed_bombs = []
            for e_nr, episode in enumerate(self.episodes):
                placed_bombs_episode = self._collected_item_episode(positions[e_nr], episode, agent_ix)
                placed_bombs.append(placed_bombs_episode)
            
            #individual plots
            if individual_plots:
                for i, e in enumerate(placed_bombs):
                    if len(agent_ixs)>1:
                        iaxs[agent_ix].plot(e, label=f"episode {i}")
                        iaxs[agent_ix].title.set_text(labels[agent_ix])
                    else:
                        iaxs.plot(e, label=f"episode {i}")

            # aggregated plot
            means, n_alive = _aggregate_timeseries(placed_bombs)
            ax1.plot(means, label="Powerups_collected", color=self.agent_colors[agent_ix])
            ax2.plot(n_alive, label="# agents alive", color=self.agent_colors[agent_ix], linestyle="dashed")
        
        ax1.set_ylabel("Powerups_collected")
        ax2.set_ylabel("Agents alive")
        ax1.set_xlabel("Steps")
        patches = [mpatches.Patch(color=self.agent_colors[ix], label=labels[ix]) for ix in agent_ixs]
        ax1.legend(handles=patches, loc='upper center', bbox_to_anchor=(0.5, -.2), fancybox=True, shadow=True, ncol=len(agent_ixs))
        fig.tight_layout()
        fig.savefig(f"collected_powerups_aggregated", bbox_inches="tight")

        if individual_plots:
            ifig.savefig(f"collected_powerups_individual", bbox_inches="tight")


    def _collected_item_episode(self, positions, episode, agent_ix):
        collected_items = [0]
        for (state, position) in zip(episode[:len(positions)+1], positions[1:]):
            collected = state[agent_ix]["board"][position] in [constants.Item.ExtraBomb.value, constants.Item.IncrRange.value, constants.Item.Kick.value]
            collected_items.append(collected_items[-1] + collected)
        return collected_items

    def plot_placed_bombs(self, agent_ixs, individual_plots=False):
        # agent labels
        labels = [self.env._agents[i].agent_name.split("Agent")[0] + f" - id:{self.env._agents[i]._character.agent_id}" for i in agent_ixs]
        #individual plots
        if individual_plots:
                ifig, iaxs = plt.subplots(1, len(agent_ixs), sharex=True, sharey=True, figsize=(16,4))

        # aggregated plot
        fig, ax1 = plt.subplots(sharex=True, sharey=True)
        ax2 = ax1.twinx()
        for agent_ix in agent_ixs:
            positions = self._visited_positions()[agent_ix]
            placed_bombs = []
            for e_nr, episode in enumerate(self.episodes):
                placed_bombs_episode = self._placed_bomb_episode(positions[e_nr], episode, agent_ix)
                placed_bombs.append(placed_bombs_episode)
            
            #individual plots
            if individual_plots:
                for i, e in enumerate(placed_bombs):
                    if len(agent_ixs)>1:
                        iaxs[agent_ix].plot(e, label=f"episode {i}")
                        iaxs[agent_ix].title.set_text(labels[agent_ix])
                    else:
                        iaxs.plot(e, label=f"episode {i}")

            # aggregated plot
            means, n_alive = _aggregate_timeseries(placed_bombs)
            ax1.plot(means, label="Bombs placed", color=self.agent_colors[agent_ix])
            ax2.plot(n_alive, label="# agents alive", color=self.agent_colors[agent_ix], linestyle="dashed")
        
        ax1.set_ylabel("Bombs placed")
        ax2.set_ylabel("Agents alive")
        ax1.set_xlabel("Steps")
        patches = [mpatches.Patch(color=self.agent_colors[ix], label=labels[ix]) for ix in agent_ixs]
        ax1.legend(handles=patches, loc='upper center', bbox_to_anchor=(0.5, -.2), fancybox=True, shadow=True, ncol=len(agent_ixs))
        fig.tight_layout()
        fig.savefig(f"placed_bombs_aggregated", bbox_inches="tight")

        if individual_plots:
            ifig.savefig(f"placed_bombs_individual", bbox_inches="tight")

    def _placed_bomb_episode(self, positions, episode, agent_ix):
        placed_bombs = [0]
        for (state, previous_position) in zip(episode[1:len(positions)], positions[:-1]):
            placed_bomb = state[agent_ix]["bomb_life"][previous_position]==9
            placed_bombs.append(placed_bombs[-1] + placed_bomb)
        return placed_bombs

    def _get_visited_positions(self):
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

    def _visited_positions(self):
        if self.visited_positions is None:
            self.visited_positions = self._get_visited_positions()
        return self.visited_positions


    def plot_explored_positions(self, agent_ixs=None):
        if agent_ixs is None:
            agent_ixs = list(range(self.n_agents))

        vp = self._visited_positions()

        # plot single episodes
        fig, ax = plt.subplots()
        for agent_ix in agent_ixs:
            visited_positions = vp[agent_ix]
            unique_ixs = [np.sort(np.unique(positions, return_index=True, axis=0)[1]) for positions in visited_positions]
            unique_states = [list(range(1,len(ixs)+1)) for ixs in unique_ixs]

            for i, (x,y) in enumerate(zip(unique_ixs, unique_states)):
                ax.plot(x,y, color=self.agent_colors[agent_ix])
        
        labels = [self.env._agents[i].agent_name.split("Agent")[0] + f" - id:{self.env._agents[i]._character.agent_id}" for i in agent_ixs]
        patches = [mpatches.Patch(color=self.agent_colors[ix], label=labels[ix]) for ix in agent_ixs]
        ax.legend(handles=patches)
        ax.set_ylabel('unique states visited')
        ax.set_xlabel(f"steps")

        fig.savefig(f"visited unique positions", bbox_inches="tight")
        plt.clf()

        # plot aggregated
        fig, ax1 = plt.subplots(constrained_layout=True)
        ax2 = ax1.twinx()

        for agent_ix in agent_ixs:
            visited_positions = vp[agent_ix]
            #max_steps = max(len(positions) for positions in visited_positions)
            us = []
            unique_ixs = [np.sort(np.unique(positions, return_index=True, axis=0)[1]) for positions in visited_positions]

            for i, ixs in enumerate(unique_ixs):
                us.append([len(ixs[ixs<=ix]) for ix in range(0,len(visited_positions[i]))])

            # ToDo: Rename variables
            varieties = np.array(list(zip_longest(*us)),dtype=float)
            means = np.nanmean(varieties, axis=1)
            n_alive = len(visited_positions) - np.count_nonzero(np.isnan(varieties),axis=1)
            
            ax1.plot(means, color=self.agent_colors[agent_ix], label="unique states" + labels[agent_ix])
            ax2.plot(n_alive, color=self.agent_colors[agent_ix], linestyle='dashed', label="alive" + labels[agent_ix])
        
        ax1.set_ylabel('unique states visited')
        ax2.set_ylabel('agents alive')

        if len(agent_ixs)>1:
            ax1.legend(handles=patches, loc='upper center', bbox_to_anchor=(0.5, -0.05), fancybox=True, shadow=True)

        ax1.set_xlabel(f"steps")
        fig.savefig(f"visited unique positions_aggregated", bbox_inches="tight")
        plt.clf()

    def plot_state_varieties(self, agent_ixs=None, max_variety=20, modes=None):
        """_summary_

        :param agent_ixs: list of agent ids that should be included in the plot, defaults to all agents
        :param max_variety:  size of sliding window which is used to count unique visited states. defaults to 20
        :param modes: list of modes for plot generation: "aggregated", "bars", "grid", "stacked", "timeseries", defaults to None
        """

        if agent_ixs is None:
            agent_ixs = list(range(self.n_agents))
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
        
        # state_varieties[agent, episode, step]
        labels = [self.env._agents[i].agent_name.split("Agent")[0] + f" - id:{self.env._agents[i]._character.agent_id}" for i in agent_ixs]
        patches = [mpatches.Patch(color=self.agent_colors[ix], label=labels[ix]) for ix in agent_ixs]
        
        for mode in modes:
            if mode in ["timeseries"]: # stacked
                _, ax = plt.subplots(sharex=True)
                ax2 = ax.twinx()
                for a_ix in agent_ixs:
                    self._plot_state_varieties((ax,ax2), state_varieties[a_ix], 20, mode=mode, color=self.agent_colors[a_ix])
                plt.xlabel(f"steps")
            else:
                if mode in ["grid"]: # horizontal
                    _, axs = plt.subplots(1, len(agent_ixs), sharex=True, sharey=True)  
                else: # vertical
                    _, axs = plt.subplots(len(agent_ixs), sharex=True, sharey=True)
        
                for a_ix in agent_ixs:
                    if type(axs)==np.ndarray or (axs.numCols*axs.numRows) > 1:
                        self._plot_state_varieties(axs[a_ix], state_varieties[a_ix], 20, mode=mode, color=self.agent_colors[a_ix])
                        ax = axs[0]
                    else:
                        self._plot_state_varieties(axs, state_varieties[a_ix], 20, mode=mode, color=self.agent_colors[a_ix])
                        ax = axs
                plt.xlabel(f"unique visited stated within {max_variety} moves")

            ax.legend(handles=patches, loc='upper left', bbox_to_anchor=(1, 1.05), fancybox=True, shadow=True)
            plt.tight_layout()
            plt.savefig(f"state_variety_window{max_variety}_episodes{len(self.episodes)}_{mode}", bbox_inches="tight")
            plt.clf()

    def _plot_state_varieties(self, ax, state_varieties: List[List[int]], max_variety, mode=None, color="blue"):
        """_summary_

        :param state_varieties: list of unique visted positions within a sliding window per episode
        :param max_variety: _description_
        :param mode: type of plot that should be generated. Possible options: "grid", "aggregated", "stacked", "
        """
        
        if mode is None:
            mode = "aggregated"

        episodes = len(state_varieties)
        if mode == "grid":
            plots_per_row = math.ceil(np.sqrt(episodes))
            _, axs = plt.subplots(plots_per_row, plots_per_row, sharex=True, sharey=True)
            for i, varieties in enumerate(state_varieties):
                axs[i%plots_per_row, int(i/plots_per_row)].hist(varieties, bins=max_variety, range=(1, max_variety+1), align="left", density=False)
            ax.set_xticks(np.arange(1, max_variety+1, 1))
        elif mode == "bars":
            ax.hist(np.array(state_varieties, dtype=object), bins=max_variety, range=(1, max_variety+1), align="left", histtype='step', fill=False)
            ax.set_xticks(np.arange(1, max_variety+1, 1))
        elif mode == "stacked":
            ax.hist(np.array(state_varieties, dtype=object), bins=max_variety, range=(1, max_variety+1), align="left", histtype='barstacked')
            ax.set_xticks(np.arange(1, max_variety+1, 1))
        elif mode == "aggregated":
            ax.hist(np.concatenate(state_varieties), bins=max_variety, range=(1, max_variety+1), align="left", density=True, color=color)
            ax.set_xticks(np.arange(1, max_variety+1, 1))
        elif mode == "timeseries":
            ax1, ax2 = ax
            means, n_alive = _aggregate_timeseries(state_varieties)
            ax1.plot(means, color=color, label="unique states")
            ax2.plot(n_alive, color=color, linestyle='dashed', label="alive")
            ax1.set_ylabel('unique states', color='black')
            ax2.set_ylabel('agents alive', color='black')

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