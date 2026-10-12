#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

inline glm::vec3 CircumCenter(const glm::vec3& a, const glm::vec3& b,
                              const glm::vec3& c) {
  glm::vec3 ab = (a + b) / 2.0f;
  glm::vec3 ac = (a + c) / 2.0f;
  glm::vec3 bc = (b + c) / 2.0f;

  glm::vec3 n1 = (b - a);
  glm::vec3 n2 = (c - a);
  glm::vec3 n3 = (c - b);
  float c1 = glm::dot(n1, ab);
  float c2 = glm::dot(n2, ac);
  float c3 = glm::dot(n3, bc);

  auto Cross = [](float a1, float b1, float c1, float a2, float b2, float c2) {
    glm::vec3 p(0.0f);
    float det = a1 * b2 - a2 * b1;
    if (std::abs(det) < 1e-6f) {
      return p;
    }

    p.x = (c1 * b2 - c2 * b1) / det;
    p.y = (a1 * c2 - a2 * c1) / det;
    return p;
  };

  return Cross(n1.x, n1.y, c1, n2.x, n2.y, c2);
}

inline std::vector<int> BowyerWatson(std::vector<glm::vec3>& points,
                                     int it = -1) {
  struct UVec3Comp {
    bool operator()(const glm::uvec3& a, const glm::uvec3& b) const {
      return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
    }
  };
  std::vector<glm::uvec3> triangles;

  points.push_back(glm::vec3(-10.0f, -10.0f, 0.0f));
  points.push_back(glm::vec3(10.0f, -10.0f, 0.0f));
  points.push_back(glm::vec3(0.0f, 10.0f, 0.0f));
  triangles.push_back(
      {points.size() - 1, points.size() - 2, points.size() - 3});

  const size_t num_input_points = points.size() - 3;

  for (size_t i = 0; (i < static_cast<size_t>(it)) && (i < num_input_points);
       ++i) {
    glm::vec3 point = points[i];
    std::set<glm::uvec3, UVec3Comp> bad_triangles;
    std::set<std::pair<int, int>> polygon;

    for (const auto& tri : triangles) {
      glm::vec3 a = points[tri.x];
      glm::vec3 b = points[tri.y];
      glm::vec3 c = points[tri.z];
      glm::vec3 center = CircumCenter(a, b, c);
      float radius = glm::distance(a, center);
      float dist = glm::distance(point, center);
      if (dist < radius) {
        bad_triangles.insert(tri);
      }
    }

    for (const auto& tri : bad_triangles) {
      std::vector<std::pair<int, int>> edges = {
          {tri.x, tri.y}, {tri.y, tri.z}, {tri.z, tri.x}};
      for (const auto& e : edges) {
        std::pair<int, int> rev = {e.second, e.first};
        if (polygon.count(rev)) {
          polygon.erase(rev);
        } else {
          polygon.insert(e);
        }
      }
    }

    triangles.erase(std::remove_if(triangles.begin(), triangles.end(),
                                   [&](const glm::uvec3& t) {
                                     return bad_triangles.count(t) > 0;
                                   }),
                    triangles.end());

    for (const auto& edge : polygon) {
      triangles.emplace_back(edge.first, edge.second, i);
    }
  }

  triangles.erase(std::remove_if(triangles.begin(), triangles.end(),
                                 [&](const glm::uvec3& t) {
                                   return (t.x >= num_input_points ||
                                           t.y >= num_input_points ||
                                           t.z >= num_input_points);
                                 }),
                  triangles.end());

  points.resize(num_input_points);

  auto GetAngle = [](const glm::vec3& a, const glm::vec3& b,
                     const glm::vec3& c) -> float {
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    float dot = glm::dot(ab, ac);
    float len = glm::length(ab) * glm::length(ac);
    if (len < 1e-6f) return 0.0f;
    return glm::degrees(glm::acos(glm::clamp(dot / len, -1.0f, 1.0f)));
  };

  auto GetMaxAngle = [&](const glm::vec3& a, const glm::vec3& b,
                         const glm::vec3& c) -> float {
    return std::max(
        {GetAngle(a, b, c), GetAngle(b, c, a), GetAngle(c, a, b)});
  };

  triangles.erase(std::remove_if(triangles.begin(), triangles.end(),
                                 [&](const glm::uvec3& t) {
                                   glm::vec3 a = points[t.x];
                                   glm::vec3 b = points[t.y];
                                   glm::vec3 c = points[t.z];
                                   return GetMaxAngle(a, b, c) > 120.0f;
                                 }),
                  triangles.end());

  std::vector<int> flat_indices;
  for (const auto& t : triangles) {
    flat_indices.push_back(t.x);
    flat_indices.push_back(t.y);
    flat_indices.push_back(t.z);
  }

  return flat_indices;
}

inline std::vector<glm::vec3> GenerateTestPositions(int size) {
  float min_val = -0.8f;
  float max_val = 0.8f;
  float step = (max_val - min_val) / static_cast<float>(size);
  float fact = step * 0.2f;
  std::vector<glm::vec3> points(size * size);
  for (int x = 0; x < size; ++x) {
    for (int y = 0; y < size; ++y) {
      float rnd = glm::linearRand(fact, step - fact);
      points[x * size + y] = {min_val + static_cast<float>(x) * step + rnd,
                              min_val + static_cast<float>(y) * step + rnd,
                              0.0f};
    }
  }
  return points;
}

template <typename IndexType = int>
class DualGraph {
 public:
  DualGraph(const std::vector<IndexType>& indices, IndexType vertex_count)
      : node_count_(indices.size() / 3), adjacency_(node_count_) {
    std::vector<int> index_appearances(vertex_count + 1, 0);
    for (auto idx : indices) {
      index_appearances[idx]++;
    }

    std::vector<int> offsets = index_appearances;
    std::exclusive_scan(offsets.begin(), offsets.end(), offsets.begin(), 0);

    std::vector<int> current_offset = offsets;
    std::vector<IndexType> faces_grouped(indices.size());

    for (size_t i = 0; i < node_count_; ++i) {
      for (size_t j = 0; j < 3; ++j) {
        IndexType v = indices[i * 3 + j];
        faces_grouped[current_offset[v]++] = static_cast<IndexType>(i);
      }
    }

    auto CheckAdjacency = [&](IndexType f1, IndexType f2) {
      int match = 0;
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
          if (indices[f1 * 3 + i] == indices[f2 * 3 + j]) match++;
        }
      }
      return match == 2;
    };

    for (size_t i = 0; i < static_cast<size_t>(vertex_count); ++i) {
      for (int j = offsets[i]; j < current_offset[i]; ++j) {
        for (int k = j + 1; k < current_offset[i]; ++k) {
          IndexType f1 = faces_grouped[j];
          IndexType f2 = faces_grouped[k];

          if (CheckAdjacency(f1, f2)) {
            if (std::find(adjacency_[f1].begin(), adjacency_[f1].end(), f2) ==
                adjacency_[f1].end()) {
              adjacency_[f1].push_back(f2);
              adjacency_[f2].push_back(f1);
            }
          }
        }
      }
    }
  }

  size_t NodeCount() const { return node_count_; }

  const std::vector<IndexType>& GetNeighbors(IndexType node) const {
    return adjacency_[node];
  }

 private:
  size_t node_count_;
  std::vector<std::vector<IndexType>> adjacency_;
};