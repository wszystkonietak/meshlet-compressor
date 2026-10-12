#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Timer.hpp"

template <typename Graph, typename IndexType>
concept PathCoverGraph =
    std::integral<IndexType> && requires(Graph graph, IndexType node) {
      { graph.NodeCount() } -> std::integral;
      { graph.GetNeighbors(node) } -> std::ranges::input_range;
    };

template <typename Graph, typename IndexType>
  requires PathCoverGraph<Graph, IndexType>
class PathCover {
 public:
  static constexpr IndexType kNullIndex = static_cast<IndexType>(-1);

  explicit PathCover(const Graph& g)
      : graph_(g),
        node_count_(g.NodeCount()),
        k_star_(node_count_, 0),
        star_nodes_(node_count_, 0),
        nodes_(node_count_),
        paths_(node_count_),
        node_ids_(node_count_),
        fake_edge_target_(node_count_, kNullIndex),
        terminals_(node_count_, true),
        nodes_order_(node_count_),
        star_nodes_tmp_(node_count_) {
    for (IndexType i = 0; i < node_count_; ++i) {
      node_ids_[i] = {i, 0};
      paths_[i].head = i;
      paths_[i].tail = i;
    }

    local_graph_.resize(node_count_);
    for (IndexType i = 0; i < node_count_; ++i) {
      for (IndexType neighbor : g.GetNeighbors(i)) {
        local_graph_[i].push_back(neighbor);
      }
    }
    star_nodes_.reserve(node_count_);
    insert_order_.reserve(node_count_);
    to_connect_.reserve(node_count_);
    to_split_.reserve(node_count_);
  }

  std::vector<std::vector<IndexType>> GeneratePaths() {
    int remaining_paths = node_count_;
    int prev_counter = -1;
    int max_k = 100;

    Timer t;
    t.Start();

    for (int k = 0; k <= max_k; ++k) {
      int counter = 0;

      for (IndexType i = 0; i < node_count_; ++i) {
        if (IsTerminal(i)) {
          if (!LaunchTunneling(i, k)) {
            counter++;
          } else {
            remaining_paths--;
          }
        }
      }

      std::string msg = "k: " + std::to_string(k) +
                        " | Remaining: " + std::to_string(remaining_paths);
      t.Step(msg);

      if (counter == prev_counter && !optimal_mode_) {
        optimal_mode_ = true;
        checked_terminals_.clear();
        k = -1;
        max_k = 10;
      }

      prev_counter = counter;

      if (remaining_paths <= 1) break;
    }

    std::vector<std::vector<IndexType>> paths;
    for (IndexType i = 0; i < node_count_; ++i) {
      if (paths_[i].head != kNullIndex) {
        std::vector<IndexType> current_path;
        IndexType current = paths_[i].head;

        while (current != kNullIndex) {
          current_path.push_back(current);
          current = nodes_[current].next;
        }
        paths.push_back(std::move(current_path));
      }
    }

    return paths;
  }

 private:
  struct Node {
    IndexType prev = kNullIndex;
    IndexType next = kNullIndex;
  };

  struct Path {
    IndexType head = kNullIndex;
    IndexType tail = kNullIndex;
  };

  struct FixData {
    IndexType terminal;
    IndexType insert;
    IndexType split;
  };

  void UpdateNodeIds(IndexType strip_id) {
    for (IndexType current = paths_[strip_id].head, it = 0;
         current != kNullIndex; current = nodes_[current].next, ++it) {
      node_ids_[current] = {strip_id, it};
    }
  }

  void ApplyChanges() {
    for (const auto& [n1, n2] : to_split_) {
      fake_edge_target_[n1] = kNullIndex;
      fake_edge_target_[n2] = kNullIndex;
      if (AdjacencyTest(n1, n2)) {
        Split(n1, n2);
      }
    }
    to_split_.clear();

    while (!to_connect_.empty()) {
      auto [n1, n2] = to_connect_.back();
      to_connect_.pop_back();

      IndexType s1 = node_ids_[n1].first;
      IndexType s2 = node_ids_[n2].first;

      if (s1 != s2) {
        bool front1 = (s1 == n1);
        bool front2 = (s2 == n2);
        ConnectStrips(s1, front1, s2, front2);
      }
    }
  }

  bool IsTerminal(IndexType id) const { return terminals_[id]; }

  bool AdjacencyTest(IndexType n1, IndexType n2) const {
    return (node_ids_[n1].first == node_ids_[n2].first) &&
           (std::abs(static_cast<long long>(node_ids_[n1].second) -
                     static_cast<long long>(node_ids_[n2].second)) == 1);
  }

  void JoinStrips(IndexType new_strip, IndexType s1, IndexType s2) {
    IndexType tail1 = paths_[s1].tail;
    IndexType head2 = paths_[s2].head;

    nodes_[tail1].next = head2;
    nodes_[head2].prev = tail1;

    paths_[new_strip].head = paths_[s1].head;
    paths_[new_strip].tail = paths_[s2].tail;

    if (new_strip != s1) paths_[s1] = {kNullIndex, kNullIndex};
    if (new_strip != s2) paths_[s2] = {kNullIndex, kNullIndex};
  }

  void ReverseStrip(IndexType strip_id) {
    paths_[strip_id].tail = paths_[strip_id].head;
    IndexType last = paths_[strip_id].head;

    for (IndexType current = paths_[strip_id].head; current != kNullIndex;
         current = nodes_[current].prev) {
      std::swap(nodes_[current].prev, nodes_[current].next);
      last = current;
    }

    paths_[strip_id].head = last;
  }

  void ConnectStrips(IndexType s1, bool front, IndexType s2, bool front2) {
    int s2_length =
        node_ids_[paths_[s2].tail].second - node_ids_[paths_[s2].head].second;
    int s1_length =
        node_ids_[paths_[s1].tail].second - node_ids_[paths_[s1].head].second;

    if (!front && front2) {
      nodes_[paths_[s1].tail].next = paths_[s2].head;
      nodes_[paths_[s2].head].prev = paths_[s1].tail;
      paths_[s1].tail = paths_[s2].tail;
      paths_[s2] = {kNullIndex, kNullIndex};
      UpdateNodeIds(s1);
    } else if (front && !front2) {
      nodes_[paths_[s2].tail].next = paths_[s1].head;
      nodes_[paths_[s1].head].prev = paths_[s2].tail;
      paths_[s2].tail = paths_[s1].tail;
      paths_[s1] = {kNullIndex, kNullIndex};
      UpdateNodeIds(s2);
    } else if (!front && !front2) {
      if (s2_length > s1_length) {
        ReverseStrip(s1);
        nodes_[paths_[s2].tail].next = paths_[s1].head;
        nodes_[paths_[s1].head].prev = paths_[s2].tail;
        paths_[s2].tail = paths_[s1].tail;
        paths_[s1] = {kNullIndex, kNullIndex};
        UpdateNodeIds(s2);
      } else {
        ReverseStrip(s2);
        nodes_[paths_[s1].tail].next = paths_[s2].head;
        nodes_[paths_[s2].head].prev = paths_[s1].tail;
        paths_[s1].tail = paths_[s2].tail;
        paths_[s2] = {kNullIndex, kNullIndex};
        UpdateNodeIds(s1);
      }
    } else {
      if (s2_length > s1_length) {
        IndexType new_id = paths_[s1].tail;
        ReverseStrip(s1);
        nodes_[paths_[s1].tail].next = paths_[s2].head;
        nodes_[paths_[s2].head].prev = paths_[s1].tail;
        paths_[s1].tail = paths_[s2].tail;
        paths_[new_id].head = paths_[s1].head;
        paths_[new_id].tail = paths_[s1].tail;
        if (new_id != s1) paths_[s1] = {kNullIndex, kNullIndex};
        paths_[s2] = {kNullIndex, kNullIndex};
        UpdateNodeIds(new_id);
      } else {
        IndexType new_id = paths_[s2].tail;
        ReverseStrip(s2);
        nodes_[paths_[s2].tail].next = paths_[s1].head;
        nodes_[paths_[s1].head].prev = paths_[s2].tail;
        paths_[s2].tail = paths_[s1].tail;
        paths_[new_id].head = paths_[s2].head;
        paths_[new_id].tail = paths_[s2].tail;
        if (new_id != s2) paths_[s2] = {kNullIndex, kNullIndex};
        paths_[s1] = {kNullIndex, kNullIndex};
        UpdateNodeIds(new_id);
      }
    }
  }

  void MakeMess(IndexType terminal, IndexType insert_node, IndexType split_node,
                bool fix_mess = false) {
    IndexType separated_strip_end1 = split_node;
    IndexType joined_strip_end1;
    IndexType terminal_strip = node_ids_[terminal].first;
    bool front = (terminal == terminal_strip);

    if (front) {
      joined_strip_end1 = paths_[terminal_strip].tail;
    } else {
      joined_strip_end1 = paths_[terminal_strip].head;
    }

    IndexType split_id = node_ids_[split_node].second;
    IndexType insert_id = node_ids_[insert_node].second;
    IndexType separated_strip_end2;
    IndexType joined_strip_end2;
    IndexType split_strip = node_ids_[split_node].first;
    bool front2 = (split_id > insert_id);

    if (terminal_strip == split_strip) {
      if (front2) {
        paths_[split_node].head = split_node;
        paths_[split_node].tail = paths_[terminal_strip].tail;
        paths_[terminal_strip].tail = insert_node;
        nodes_[split_node].prev = kNullIndex;
        nodes_[insert_node].next = kNullIndex;
        ConnectStrips(terminal_strip, false, split_node, false);
      } else {
        paths_[insert_node].head = insert_node;
        paths_[insert_node].tail = paths_[terminal_strip].tail;
        paths_[terminal_strip].tail = split_node;
        nodes_[split_node].next = kNullIndex;
        nodes_[insert_node].prev = kNullIndex;
        ConnectStrips(terminal_strip, true, insert_node, true);
      }
      if (!fix_mess) {
        insert_order_.push_back({split_node, insert_node, terminal});
      }
      return;
    }

    if (front2) {
      separated_strip_end2 = paths_[split_strip].tail;
      joined_strip_end2 = paths_[split_strip].head;
    } else {
      separated_strip_end2 = paths_[split_strip].head;
      joined_strip_end2 = paths_[split_strip].tail;
    }

    if (front2) {
      paths_[separated_strip_end1].head = separated_strip_end1;
      paths_[separated_strip_end1].tail = separated_strip_end2;
      nodes_[separated_strip_end1].prev = kNullIndex;
      nodes_[separated_strip_end2].next = kNullIndex;
      if (separated_strip_end1 != separated_strip_end2) {
        paths_[separated_strip_end2] = {kNullIndex, kNullIndex};
      }
      UpdateNodeIds(separated_strip_end1);
    } else {
      paths_[separated_strip_end2].head = separated_strip_end2;
      paths_[separated_strip_end2].tail = separated_strip_end1;
      nodes_[separated_strip_end2].prev = kNullIndex;
      nodes_[separated_strip_end1].next = kNullIndex;
      UpdateNodeIds(separated_strip_end2);
    }

    IndexType tmp_strip_end = insert_node;
    IndexType tmp_strip_end2 = joined_strip_end2;
    if (front2) std::swap(tmp_strip_end, tmp_strip_end2);

    paths_[tmp_strip_end].head = tmp_strip_end;
    paths_[tmp_strip_end].tail = tmp_strip_end2;
    nodes_[tmp_strip_end].prev = kNullIndex;
    nodes_[tmp_strip_end2].next = kNullIndex;

    bool terminal_strip_front_connect = !front;
    bool split_strip_front_connect = front2;
    ConnectStrips(terminal_strip, !terminal_strip_front_connect, tmp_strip_end,
                  !split_strip_front_connect);

    if (!fix_mess) {
      insert_order_.push_back({split_node, insert_node, terminal});
    }
  }

  void Split(IndexType node1, IndexType node2) {
    if (!AdjacencyTest(node1, node2)) return;

    IndexType strip = node_ids_[node1].first;
    IndexType id1 = node_ids_[node2].second;
    IndexType id2 = node_ids_[node1].second;

    if (id1 > id2) std::swap(node1, node2);

    paths_[node1].tail = paths_[strip].tail;
    paths_[strip].tail = node2;
    nodes_[node2].next = kNullIndex;

    paths_[node1].head = node1;
    nodes_[node1].prev = kNullIndex;

    UpdateNodeIds(node1);
  }

  void FixMess() {
    auto d = insert_order_.back();
    insert_order_.pop_back();
    MakeMess(d.terminal, d.insert, d.split, true);
  }

  int LocalizeStrips(int k) {
    int local_terminals_count = 0;
    std::vector<IndexType> inside_k_star;
    std::vector<IndexType> outside_k_star;
    std::unordered_map<IndexType, std::vector<IndexType>> tmp_edges;

    for (const auto& node : star_nodes_) {
      if (IsTerminal(node)) {
        local_terminals_count++;
      }

      IndexType black_edges[2] = {nodes_[node].prev, nodes_[node].next};
      for (IndexType connection : black_edges) {
        if (connection != kNullIndex && !KStarContains(connection)) {
          tmp_edges[node_ids_[node].first].push_back(node);
          inside_k_star.push_back(node);
          outside_k_star.push_back(connection);
        }
      }
    }

    for (auto& [strip_id, cross_ids] : tmp_edges) {
      std::ranges::sort(cross_ids, std::less<>{},
                        [&](IndexType id) { return node_ids_[id].second; });
      bool starts_outside = !KStarContains(strip_id);
      size_t start_index = starts_outside ? 1 : 0;

      for (size_t i = start_index; i + 1 < cross_ids.size(); i += 2) {
        to_split_.emplace_back(cross_ids[i], cross_ids[i + 1]);
      }
    }

    for (size_t i = 0; i < inside_k_star.size(); i++) {
      to_connect_.emplace_back(inside_k_star[i], outside_k_star[i]);
      Split(inside_k_star[i], outside_k_star[i]);
    }

    for (auto& p : to_split_) {
      JoinStrips(node_ids_[p.first].first, node_ids_[p.first].first,
                 node_ids_[p.second].first);
      UpdateNodeIds(node_ids_[p.first].first);
    }
    return local_terminals_count;
  }

  bool KStarContains(IndexType k) const {
    return k_star_[k] >= k_star_version_;
  }

  void KStarInsert(IndexType k) { k_star_[k] = k_star_version_; }

  void KStarClear() {
    k_star_version_ += 1 + k_star_subgroup_version_;
    k_star_subgroup_version_ = 1;
  }

  bool OptimalKStarFind(IndexType terminal, int k) {
    KStarClear();
    star_nodes_tmp_.clear();
    star_nodes_tmp_.emplace_back(terminal, kNullIndex);
    KStarInsert(terminal);

    int current_id[] = {1, 0};
    IndexType second_end = kNullIndex;
    bool found = false;
    int length = 0;

    while (current_id[0] != current_id[1]) {
      current_id[length % 2] = static_cast<int>(star_nodes_tmp_.size());

      for (int j = current_id[(length + 1) % 2]; j < current_id[length % 2];
           ++j) {
        IndexType current_node = star_nodes_tmp_[j].first;

        for (IndexType neighbour : graph_.GetNeighbors(current_node)) {
          if (!KStarContains(neighbour)) {
            KStarInsert(neighbour);
            star_nodes_tmp_.emplace_back(neighbour, static_cast<IndexType>(j));

            if (neighbour == terminal) continue;
            if (!IsTerminal(neighbour)) continue;

            if (checked_terminals_.contains(terminal) &&
                checked_terminals_[terminal].contains(neighbour)) {
              continue;
            }

            second_end = neighbour;
            found = true;
            break;
          }
        }
        if (found) break;
      }
      if (found) break;
      length++;
    }

    if (!found) return false;

    checked_terminals_[terminal].insert(second_end);
    KStarClear();
    star_nodes_.clear();

    IndexType current_idx = static_cast<IndexType>(star_nodes_tmp_.size() - 1);
    while (current_idx != kNullIndex) {
      IndexType node = star_nodes_tmp_[current_idx].first;
      star_nodes_.push_back(node);
      KStarInsert(node);
      current_idx = star_nodes_tmp_[current_idx].second;
    }

    current_id[0] = static_cast<int>(star_nodes_.size());
    current_id[1] = 0;

    for (int i = 0; i < expansion_width_; ++i) {
      current_id[i % 2] = static_cast<int>(star_nodes_.size());

      for (int j = current_id[(i + 1) % 2]; j < current_id[i % 2]; ++j) {
        for (IndexType neighbour : graph_.GetNeighbors(star_nodes_[j])) {
          if (!KStarContains(neighbour)) {
            KStarInsert(neighbour);
            star_nodes_.push_back(neighbour);
          }
        }
      }
    }

    KStarSubgroupClear();
    current_id[0] = 1;
    current_id[1] = 0;
    star_nodes_.clear();
    star_nodes_.push_back(second_end);
    nodes_order_[second_end] = 0;
    KStarSubgroupInsert(second_end);

    int it = 0;
    while (current_id[1] != current_id[0]) {
      current_id[it % 2] = static_cast<int>(star_nodes_.size());

      for (int j = current_id[(it + 1) % 2]; j < current_id[it % 2]; ++j) {
        IndexType current_node = star_nodes_[j];
        for (IndexType neighbour : graph_.GetNeighbors(current_node)) {
          if (!KStarSubgroupContains(neighbour) && KStarContains(neighbour)) {
            KStarSubgroupInsert(neighbour);
            star_nodes_.push_back(neighbour);
            nodes_order_[neighbour] = nodes_order_[current_node] + 1;
          }
        }
      }
      it++;
    }
    KStarSubgroupClear();

    return true;
  }

  void KStarFind(IndexType terminal, int k) {
    KStarClear();
    star_nodes_.clear();
    star_nodes_.push_back(terminal);
    KStarInsert(terminal);

    int current_id[] = {0, 0};
    for (int i = 0; i < k; i++) {
      current_id[i % 2] = static_cast<int>(star_nodes_.size());

      for (int j = current_id[(i + 1) % 2]; j < current_id[i % 2]; j++) {
        for (auto neighbour : graph_.GetNeighbors(star_nodes_[j])) {
          if (!KStarContains(neighbour)) {
            KStarInsert(neighbour);
            star_nodes_.push_back(neighbour);
          }
        }
      }
    }
  }

  bool KStarSubgroupContains(IndexType k) const {
    return k_star_[k] == k_star_version_ + k_star_subgroup_version_;
  }

  void KStarSubgroupInsert(IndexType k) {
    k_star_[k] = k_star_version_ + k_star_subgroup_version_;
  }

  void KStarSubgroupClear() { k_star_subgroup_version_++; }

  void KStarSubgroupErase(IndexType k) { k_star_[k]--; }

  bool LaunchTunneling(IndexType terminal, int k) {
    struct SearchData {
      int depth;
      int visited_terminals_count;
      FixData data;
    };

    if (optimal_mode_) {
      if (!OptimalKStarFind(terminal, k)) return false;
    } else {
      KStarFind(terminal, k);
    }

    int local_terminals_count = LocalizeStrips(k);
    std::vector<IndexType> visited_order;
    visited_order.reserve(star_nodes_.size());

    std::vector<SearchData> search_stack;
    search_stack.reserve(max_depth_ + 2);

    search_stack.push_back({0, 1, {terminal, terminal, terminal}});
    visited_order.push_back(terminal);

    KStarSubgroupClear();
    KStarSubgroupInsert(terminal);

    bool connection_found = false;
    int counter = 0;
    IndexType second_end = kNullIndex;

    while (!search_stack.empty()) {
      auto current_state = search_stack.back();
      search_stack.pop_back();
      counter++;
      if (counter == max_count_) break;
      if (current_state.depth == max_depth_) continue;

      while (insert_order_.size() >
             static_cast<size_t>(current_state.depth - 1)) {
        FixMess();
      }

      if (current_state.depth > 0) {
        while (visited_order.size() >
               static_cast<size_t>(2 * (current_state.depth - 1) + 1)) {
          KStarSubgroupErase(visited_order.back());
          visited_order.pop_back();
        }
        MakeMess(current_state.data.terminal, current_state.data.insert,
                 current_state.data.split);

        KStarSubgroupInsert(current_state.data.insert);
        visited_order.push_back(current_state.data.insert);

        KStarSubgroupInsert(current_state.data.split);
        visited_order.push_back(current_state.data.split);
      }

      IndexType new_terminal_node = current_state.data.split;
      auto [terminal_strip, terminal_id] = node_ids_[new_terminal_node];
      bool front = (terminal_strip == new_terminal_node);

      if (optimal_mode_) {
        std::ranges::sort(local_graph_[new_terminal_node], std::greater<>{},
                          [&](IndexType n) { return nodes_order_[n]; });
      }
      for (IndexType new_insert_node : local_graph_[new_terminal_node]) {
        if (!KStarContains(new_insert_node) ||
            KStarSubgroupContains(new_insert_node))
          continue;

        if (AdjacencyTest(new_terminal_node, new_insert_node)) continue;

        auto [split_strip, split_id] = node_ids_[new_insert_node];
        bool front2 = (split_strip == new_insert_node);
        bool same_strip = (terminal_strip == split_strip);

        if (!same_strip && IsTerminal(new_insert_node)) {
          ConnectStrips(terminal_strip, front, split_strip, front2);
          second_end = new_insert_node;
          connection_found = true;
          break;
        }

        if (optimal_mode_) {
          std::ranges::sort(local_graph_[new_insert_node], std::greater<>{},
                            [&](IndexType n) { return nodes_order_[n]; });
        }

        for (IndexType new_split_node : local_graph_[new_insert_node]) {
          if (!KStarContains(new_split_node) ||
              KStarSubgroupContains(new_split_node))
            continue;

          if (!AdjacencyTest(new_split_node, new_insert_node)) {
            continue;
          }

          if (fake_edge_target_[new_insert_node] == new_split_node) {
            continue;
          }

          if (same_strip &&
              front == (split_id < node_ids_[new_split_node].second))
            continue;

          int visited_terminals = current_state.visited_terminals_count +
                                  IsTerminal(new_split_node) +
                                  IsTerminal(new_insert_node);
          if (visited_terminals == local_terminals_count) continue;

          search_stack.push_back(
              {current_state.depth + 1,
               visited_terminals,
               {new_terminal_node, new_insert_node, new_split_node}});
        }
      }

      if (connection_found) break;
    }

    if (!connection_found) {
      while (!insert_order_.empty()) {
        FixMess();
      }
    }

    insert_order_.clear();
    ApplyChanges();

    if (connection_found) {
      if (nodes_[terminal].prev != kNullIndex &&
          nodes_[terminal].next != kNullIndex)
        terminals_[terminal] = false;
      if (nodes_[second_end].prev != kNullIndex &&
          nodes_[second_end].next != kNullIndex)
        terminals_[second_end] = false;
    }

    return connection_found;
  }

  const Graph& graph_;
  IndexType node_count_;
  std::vector<IndexType> k_star_;
  std::vector<IndexType> star_nodes_;
  int k_star_version_ = 1;
  int k_star_subgroup_version_ = 1;

  int max_count_ = 10000;
  int max_depth_ = 250;
  int expansion_width_ = 20;
  bool optimal_mode_ = false;

  std::vector<Node> nodes_;
  std::vector<Path> paths_;
  std::vector<std::pair<IndexType, IndexType>> node_ids_;
  std::vector<FixData> insert_order_;
  std::vector<std::pair<IndexType, IndexType>> to_connect_;
  std::vector<std::pair<IndexType, IndexType>> to_split_;
  std::vector<bool> terminals_;

  std::vector<int> nodes_order_;
  std::vector<std::pair<IndexType, IndexType>> star_nodes_tmp_;
  std::unordered_map<IndexType, std::unordered_set<IndexType>>
      checked_terminals_;
  std::vector<IndexType> fake_edge_target_;
  std::vector<std::vector<IndexType>> local_graph_;
};