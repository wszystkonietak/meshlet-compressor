#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <ranges>
#include <unordered_map>
#include <vector>

template <typename Graph, typename IndexType>
concept PathCoverGraph =
    std::integral<IndexType> && requires(Graph graph, IndexType node) {
      { graph.node_count() } -> std::integral;
      { graph.get_neighbors(node) } -> std::ranges::input_range;
    };

template <typename Graph, typename IndexType>
  requires PathCoverGraph<Graph, IndexType>
class PathCover {
 public:
  static constexpr IndexType NullIndex = static_cast<IndexType>(-1);
  PathCover(const Graph& g)
      : graph(g),
        _node_count(g.node_count()),
        _k_star(_node_count, 0),
        _star_nodes(_node_count, 0),
        _nodes(_node_count),
        _paths(_node_count),
        _node_ids(_node_count),
        _terminals(_node_count, true) {
    for (IndexType i = 0; i < _node_count; ++i) {
      _node_ids[i] = {i, 0};

      _paths[i].head = i;
      _paths[i].tail = i;
    }

    _star_nodes.reserve(_node_count);
    _insert_order.reserve(_node_count);
    _to_connect.reserve(_node_count);
    _to_split.reserve(_node_count);
  }

  std::vector<std::vector<IndexType>> GeneratePaths() {
    int prev_counter = -1;
    bool first_iter = true;

    int remaining_paths = _node_count;

    for (int k = 0; k <= 50; ++k) {
      int counter = 0;

      for (IndexType i = 0; i < _node_count; ++i) {
        if (IsTerminal(i)) {
          if (!LaunchTunneling(i, k)) {
            counter++;
          } else {
            remaining_paths--;
            std::cout<<remaining_paths<<'\n';
          }
        }
      }

      // if (counter != prev_counter) {
      //   first_iter = false;
      //   k--;
      // } else {
      //   first_iter = true;
      // }
      // prev_counter = counter;

      if (remaining_paths <= 1) break;
    }

    std::vector<std::vector<IndexType>> paths;
    for (IndexType i = 0; i < _node_count; ++i) {
      if (_paths[i].head != NullIndex) {
        std::vector<IndexType> current_path;
        IndexType current = _paths[i].head;

        while (current != NullIndex) {
          current_path.push_back(current);
          current = _nodes[current].next;
        }
        paths.push_back(std::move(current_path));
      }
    }

    return paths;
  }

  const Graph& graph;

 private:
  IndexType _node_count;
  std::vector<IndexType> _k_star;
  std::vector<IndexType> _star_nodes;
  int _k_star_version = 1;
  int _k_star_subgroup_version = 1;
  int _max_count = 10000;
  int _max_depth = 200;

  struct node {
    IndexType prev = NullIndex, next = NullIndex;
  };
  struct path {
    IndexType head = NullIndex, tail = NullIndex;
  };
  struct fix_data {
    IndexType terminal, insert, split;
  };

  std::vector<node> _nodes;
  std::vector<path> _paths;
  std::vector<std::pair<IndexType, IndexType>> _node_ids;
  std::vector<fix_data> _insert_order;
  std::vector<std::pair<int, int>> _to_connect;
  std::vector<std::pair<int, int>> _to_split;
  std::vector<bool> _terminals;

  void UpdateNodeIds(IndexType strip_id) {
    for (IndexType current = _paths[strip_id].head, it = 0;
         current != NullIndex; current = _nodes[current].next, ++it) {
      _node_ids[current] = {strip_id, it};
    }
  }

  void ApplyChanges() {              
    for (const auto& [n1, n2] : _to_split) {
      Split(n1, n2);
    }
    _to_split.clear();

    while (!_to_connect.empty()) {
      auto [n1, n2] = _to_connect.back();
      _to_connect.pop_back();

      IndexType s1 = _node_ids[n1].first;
      IndexType s2 = _node_ids[n2].first;
      
      bool front1 = (s1 == n1);
      bool front2 = (s2 == n2);

      ConnectStrips(s1, front1, s2, front2);
    }
  }

  bool IsTerminal(IndexType id) {
    return _terminals[id];
  }

  bool IsRedEdge(IndexType n1, IndexType n2) {
    return n1 != _nodes[n2].prev && n1 != _nodes[n2].next;
  }

  void JoinStrips(IndexType newStrip, IndexType s1, IndexType s2) {
    IndexType tail1 = _paths[s1].tail;
    IndexType head2 = _paths[s2].head;

    _nodes[tail1].next = head2;
    _nodes[head2].prev = tail1;

    _paths[newStrip].head = _paths[s1].head;
    _paths[newStrip].tail = _paths[s2].tail;

    if (newStrip != s1) _paths[s1] = {NullIndex, NullIndex};
    if (newStrip != s2) _paths[s2] = {NullIndex, NullIndex};
  }

  void ReverseStrip(IndexType strip_id) {
    _paths[strip_id].tail = _paths[strip_id].head;
    IndexType last = _paths[strip_id].head;

    for (IndexType current = _paths[strip_id].head; current != NullIndex;
         current = _nodes[current].prev) {
      std::swap(_nodes[current].prev, _nodes[current].next);
      last = current;
    }

    _paths[strip_id].head = last;
  }

  void ConnectStrips(IndexType s1, bool front, IndexType s2, bool front2) {
    int s2_length =
        _node_ids[_paths[s2].tail].second - _node_ids[_paths[s2].head].second;
    int s1_length =
        _node_ids[_paths[s1].tail].second - _node_ids[_paths[s1].head].second;

    if (!front && front2) {
      _nodes[_paths[s1].tail].next = _paths[s2].head;
      _nodes[_paths[s2].head].prev = _paths[s1].tail;
      _paths[s1].tail = _paths[s2].tail;
      _paths[s2] = {NullIndex, NullIndex};
      UpdateNodeIds(s1);
    } else if (front && !front2) {
      _nodes[_paths[s2].tail].next = _paths[s1].head;
      _nodes[_paths[s1].head].prev = _paths[s2].tail;
      _paths[s2].tail = _paths[s1].tail;
      _paths[s1] = {NullIndex, NullIndex};
      UpdateNodeIds(s2);
    } else if (!front && !front2) {
      if (s2_length > s1_length) {
        ReverseStrip(s1);
        _nodes[_paths[s2].tail].next = _paths[s1].head;
        _nodes[_paths[s1].head].prev = _paths[s2].tail;
        _paths[s2].tail = _paths[s1].tail;
        _paths[s1] = {NullIndex, NullIndex};
        UpdateNodeIds(s2);
      } else {
        ReverseStrip(s2);
        _nodes[_paths[s1].tail].next = _paths[s2].head;
        _nodes[_paths[s2].head].prev = _paths[s1].tail;
        _paths[s1].tail = _paths[s2].tail;
        _paths[s2] = {NullIndex, NullIndex};
        UpdateNodeIds(s1);
      }
    } else {
      if (s2_length > s1_length) {
        IndexType new_id = _paths[s1].tail;
        ReverseStrip(s1);
        _nodes[_paths[s1].tail].next = _paths[s2].head;
        _nodes[_paths[s2].head].prev = _paths[s1].tail;
        _paths[s1].tail = _paths[s2].tail;
        _paths[new_id].head = _paths[s1].head;
        _paths[new_id].tail = _paths[s1].tail;
        if (new_id != s1) _paths[s1] = {NullIndex, NullIndex};
        _paths[s2] = {NullIndex, NullIndex};
        UpdateNodeIds(new_id);
      } else {
        IndexType new_id = _paths[s2].tail;
        ReverseStrip(s2);
        _nodes[_paths[s2].tail].next = _paths[s1].head;
        _nodes[_paths[s1].head].prev = _paths[s2].tail;
        _paths[s2].tail = _paths[s1].tail;
        _paths[new_id].head = _paths[s2].head;
        _paths[new_id].tail = _paths[s2].tail;
        if (new_id != s2) _paths[s2] = {NullIndex, NullIndex};
        _paths[s1] = {NullIndex, NullIndex};
        UpdateNodeIds(new_id);
      }
    }
  }

  void MakeMess(IndexType terminal, IndexType insert_node, IndexType split_node,
                bool fix_mess = false) {
    IndexType new_terminal, new_insert, new_split;

    IndexType separated_strip_end1 = split_node;
    IndexType joined_strip_end1;
    IndexType terminal_strip = _node_ids[terminal].first;
    bool front = (terminal == terminal_strip);

    if (front) {
      joined_strip_end1 = _paths[terminal_strip].tail;
    } else {
      joined_strip_end1 = _paths[terminal_strip].head;
    }

    IndexType split_id = _node_ids[split_node].second;
    IndexType insert_id = _node_ids[insert_node].second;
    IndexType separated_strip_end2, joined_strip_end2;
    IndexType split_strip = _node_ids[split_node].first;
    bool front2 = (split_id > insert_id);

    if (terminal_strip == split_strip) {
      if (front2) {
        _paths[split_node].head = split_node;
        _paths[split_node].tail = _paths[terminal_strip].tail;
        _paths[terminal_strip].tail = insert_node;
        _nodes[split_node].prev = NullIndex;
        _nodes[insert_node].next = NullIndex;
        ConnectStrips(terminal_strip, false, split_node, false);
      } else {
        _paths[insert_node].head = insert_node;
        _paths[insert_node].tail = _paths[terminal_strip].tail;
        _paths[terminal_strip].tail = split_node;
        _nodes[split_node].next = NullIndex;
        _nodes[insert_node].prev = NullIndex;
        ConnectStrips(terminal_strip, true, insert_node, true);
      }
      if (!fix_mess) {
        _insert_order.push_back({split_node, insert_node, terminal});
      }
      return;
    }

    if (front2) {
      separated_strip_end2 = _paths[split_strip].tail;
      joined_strip_end2 = _paths[split_strip].head;
    } else {
      separated_strip_end2 = _paths[split_strip].head;
      joined_strip_end2 = _paths[split_strip].tail;
    }

    if (front2) {
      _paths[separated_strip_end1].head = separated_strip_end1;
      _paths[separated_strip_end1].tail = separated_strip_end2;
      _nodes[separated_strip_end1].prev = NullIndex;
      _nodes[separated_strip_end2].next = NullIndex;
      if (separated_strip_end1 != separated_strip_end2) {
        _paths[separated_strip_end2] = {NullIndex, NullIndex};
      }
      UpdateNodeIds(separated_strip_end1);
    } else {
      _paths[separated_strip_end2].head = separated_strip_end2;
      _paths[separated_strip_end2].tail = separated_strip_end1;
      _nodes[separated_strip_end2].prev = NullIndex;
      _nodes[separated_strip_end1].next = NullIndex;
      UpdateNodeIds(separated_strip_end2);
    }

    IndexType tmp_strip_end = insert_node;
    IndexType tmp_strip_end2 = joined_strip_end2;
    if (front2) std::swap(tmp_strip_end, tmp_strip_end2);

    _paths[tmp_strip_end].head = tmp_strip_end;
    _paths[tmp_strip_end].tail = tmp_strip_end2;
    _nodes[tmp_strip_end].prev = NullIndex;
    _nodes[tmp_strip_end2].next = NullIndex;

    bool terminal_strip_front_connect = !front;
    bool split_strip_front_connect = front2;
    ConnectStrips(terminal_strip, !terminal_strip_front_connect, tmp_strip_end,
                  !split_strip_front_connect);

    if (!fix_mess) {
      _insert_order.push_back({split_node, insert_node, terminal});
    }
  }

  void Split(IndexType node1, IndexType node2) {    
    IndexType strip = _node_ids[node1].first;
    IndexType id1 = _node_ids[node2].second;
    IndexType id2 = _node_ids[node1].second;

    if (id1 > id2) std::swap(node1, node2);

    _paths[node1].tail = _paths[strip].tail;
    _paths[strip].tail = node2;
    _nodes[node2].next = NullIndex;

    _paths[node1].head = node1;
    _nodes[node1].prev = NullIndex;

    UpdateNodeIds(node1);
  }

  void FixMess() {
    auto d = _insert_order.back();
    _insert_order.pop_back();
    MakeMess(d.terminal, d.insert, d.split, true);
  }

  int LocalizeStrips(int k) {
    int local_terminals_count = 0;
    std::vector<IndexType> inside_k_star;
    std::vector<IndexType> outside_k_star;
    std::unordered_map<IndexType, std::vector<IndexType>> tmp_edges;

    for (const auto& node : _star_nodes) {
      if (IsTerminal(node)) {
        local_terminals_count++;
      }

      IndexType black_edges[2] = {_nodes[node].prev, _nodes[node].next};
      for (IndexType connection : black_edges) {
        if (connection != NullIndex &&
            !KStarContains(connection)) {
                    
          tmp_edges[_node_ids[node].first].push_back(node);
          inside_k_star.push_back(node);
          outside_k_star.push_back(connection);
        }
      }
    }

    for (auto& [strip_id, cross_ids] : tmp_edges) {
      std::ranges::sort(cross_ids, std::less<>{},
                        [&](IndexType id) { return _node_ids[id].second; });
      bool starts_outside = !KStarContains(strip_id);
      size_t start_index = starts_outside ? 1 : 0;

      for (size_t i = start_index; i + 1 < cross_ids.size(); i += 2) {
        _to_split.emplace_back(cross_ids[i], cross_ids[i + 1]);
      }
    }

    for (size_t i = 0; i < inside_k_star.size(); i++) {
      _to_connect.emplace_back(inside_k_star[i], outside_k_star[i]);
      Split(inside_k_star[i], outside_k_star[i]);
    }

    for (auto& p : _to_split) {
      JoinStrips(_node_ids[p.first].first, _node_ids[p.first].first,
                 _node_ids[p.second].first);
      UpdateNodeIds(_node_ids[p.first].first);
    }
    return local_terminals_count;
  }

  bool KStarContains(IndexType k) const {
    return _k_star[k] >= _k_star_version;
  }

  void KStarInsert(IndexType k) { _k_star[k] = _k_star_version; }

  void KStarClear() {
    _k_star_version += 1 + _k_star_subgroup_version;
    _k_star_subgroup_version = 1;
  }

  void KStarFind(IndexType terminal, int k) {
    KStarClear();
    _star_nodes.clear();
    _star_nodes.push_back(terminal);
    KStarInsert(terminal);

    int current_id[] = {0, 0};
    for (int i = 0; i < k; i++) {
      current_id[i % 2] = _star_nodes.size();

      for (int j = current_id[(i + 1) % 2]; j < current_id[i % 2]; j++) {
        for (auto neighbour : graph.get_neighbors(_star_nodes[j])) {
          if (!KStarContains(neighbour)) {
            KStarInsert(neighbour);
            _star_nodes.push_back(neighbour);
          }
        }
      }
    }
  }

  bool KStarSubgroupContains(IndexType k) const {
    return _k_star[k] == _k_star_version + _k_star_subgroup_version;
  }

  void KStarSubgroupInsert(IndexType k) {
    _k_star[k] = _k_star_version + _k_star_subgroup_version;
  }

  void KStarSubgroupClear() { _k_star_subgroup_version++; }

  void KStarSubgroupErase(IndexType k) { _k_star[k]--; }

  bool LaunchTunneling(IndexType terminal, int k) {
    if(!Validate()) abort();
    struct search_data {
      int depth;
      int visited_terminals_count;
      fix_data data;
    };

    KStarFind(terminal, k);
    int local_terminals_count = LocalizeStrips(k);
    std::vector<IndexType> visited_order;
    visited_order.reserve(_star_nodes.size());
    
    std::vector<search_data> search_stack;
    search_stack.reserve(_max_depth + 2); 

    search_stack.push_back({0, 1, {terminal, terminal, terminal}});
    visited_order.push_back(terminal);

    KStarSubgroupClear();
    KStarSubgroupInsert(terminal);

    bool connection_found = false;
    int counter = 0;
    IndexType second_end = NullIndex;

    while (!search_stack.empty()) {
      auto current_state = search_stack.back();
      search_stack.pop_back();
      counter++;
      if (counter == _max_count) break;
      if (current_state.depth == _max_depth) continue;

      while (_insert_order.size() > current_state.depth - 1) {
        FixMess();
      }

      if (current_state.depth > 0) {
        while (visited_order.size() >
               (2 * (current_state.depth - 1) + 1)) {
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
      auto [terminal_strip, terminal_id] = _node_ids[new_terminal_node];
      bool front = (terminal_strip == new_terminal_node);

      std::array<IndexType, 2> red_edges = {NullIndex, NullIndex};
      size_t red_count = 0;
      
      for (IndexType neighbor : graph.get_neighbors(new_terminal_node)) {
          if (IsRedEdge(new_terminal_node, neighbor)) {
              if (red_count < 2) red_edges[red_count++] = neighbor;
          }
      }

      if (red_count == 2 && std::rand() % 2 != 0) {
        std::swap(red_edges[0], red_edges[1]);
      }

      for (size_t i = 0; i < red_count; ++i) {
        IndexType new_insert_node = red_edges[i];

        if (!KStarContains(new_insert_node) ||
            KStarSubgroupContains(new_insert_node))
          continue;

        auto [split_strip, split_id] = _node_ids[new_insert_node];
        bool front2 = (split_strip == new_insert_node);
        bool same_strip = (terminal_strip == split_strip);

        if (!same_strip && IsTerminal(new_insert_node)) {
          ConnectStrips(terminal_strip, front, split_strip, front2);
          second_end = new_insert_node;
          connection_found = true;
          break;
        }

        std::array<IndexType, 2> possible_splits = {NullIndex, NullIndex};
        size_t split_count = 0;

        for (IndexType neighbor : graph.get_neighbors(new_insert_node)) {
          if (neighbor == _nodes[new_insert_node].prev || 
              neighbor == _nodes[new_insert_node].next) {
            possible_splits[split_count++] = neighbor;
          }
        }

        if (split_count == 2 && std::rand() % 2 != 0) {
          std::swap(possible_splits[0], possible_splits[1]);
        }

        for (size_t i = 0; i < split_count; ++i) {
          IndexType new_split_node = possible_splits[i];

          if (!KStarContains(new_split_node) ||
              KStarSubgroupContains(new_split_node))
            continue;

          if (same_strip &&
              front == (split_id < _node_ids[new_split_node].second))
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
      while (!_insert_order.empty()) FixMess();
    }

    _insert_order.clear();
    ApplyChanges();

    if (connection_found) {
      if (_nodes[terminal].prev != NullIndex && _nodes[terminal].next != NullIndex) _terminals[terminal] = false;
      if (_nodes[second_end].prev != NullIndex && _nodes[second_end].next != NullIndex) _terminals[second_end] = false;
    }

    return connection_found;
  }
  bool Validate() const {
    std::vector<bool> visited(_node_count, false);
    IndexType visited_count = 0;

    for (IndexType i = 0; i < _node_count; ++i) {
      IndexType next_node = _nodes[i].next;
      IndexType prev_node = _nodes[i].prev;

      if (next_node != NullIndex) {
        if (_nodes[next_node].prev != i) {
          std::cerr << "Bidirectional mismatch: node " << i << " points to next "
                    << next_node << ", but its prev is " << _nodes[next_node].prev << '\n';
          return false;
        }

        bool is_adjacent = false;
        for (IndexType neighbor : graph.get_neighbors(i)) {
          if (neighbor == next_node) {
            is_adjacent = true;
            break;
          }
        }
        if (!is_adjacent) {
          std::cerr << "Adjacency mismatch: node " << i << " and " << next_node
                    << " are connected in a strip but do not share a face in the graph.\n";
          return false;
        }
      }

      if (prev_node != NullIndex) {
        if (_nodes[prev_node].next != i) {
          std::cerr << "Bidirectional mismatch: node " << i << " points to prev "
                    << prev_node << ", but its next is " << _nodes[prev_node].next << '\n';
          return false;
        }
      }
    }

    for (IndexType i = 0; i < _node_count; ++i) {
      if (_paths[i].head != NullIndex) {
        IndexType current = _paths[i].head;
        IndexType expected_tail = _paths[i].tail;
        IndexType position = 0;

        if (_nodes[current].prev != NullIndex) {
          std::cerr << "Path structure error: head of path " << i 
                    << " (node " << current << ") has a non-null prev pointer.\n";
          return false;
        }

        IndexType last_valid = NullIndex;
        while (current != NullIndex) {
          if (visited[current]) {
            std::cerr << "Cycle or overlap detected at node " << current << ".\n";
            return false;
          }
          visited[current] = true;
          visited_count++;

          if (_node_ids[current].first != i || _node_ids[current].second != position) {
            std::cerr << "Node ID mismatch at node " << current 
                      << ". Expected {" << i << ", " << position 
                      << "}, got {" << _node_ids[current].first << ", " 
                      << _node_ids[current].second << "}.\n";
            return false;
          }

          last_valid = current;
          current = _nodes[current].next;
          position++;
        }

        if (last_valid != expected_tail) {
          std::cerr << "Tail mismatch in path " << i << ". Expected tail " 
                    << expected_tail << ", but path ended at " << last_valid << ".\n";
          return false;
        }
      }
    }

    if (visited_count != _node_count) {
      std::cerr << "Coverage error: Only " << visited_count << " out of " 
                << _node_count << " nodes belong to a valid path.\n";
      return false;
    }

    return true;
  }
};