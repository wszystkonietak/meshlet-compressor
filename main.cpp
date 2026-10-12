#include <algorithm>
#include <glm/glm.hpp>
#include <numeric>
#include <string>
#include <vector>

#include "Eta.hpp"
#include "MeshViewer.hpp"
#include "Utils.hpp"
#include "fast_obj.h"
#include "meshoptimizer.h"

int main(int argc, char** argv) {
  bool single_mesh = true;
  std::string obj_path;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "meshlets") {
      single_mesh = false;
    } else {
      obj_path = argv[i];
    }
  }

  std::vector<glm::vec3> uncompressed_vertices;
  std::vector<unsigned int> uncompressed_indices;

  if (obj_path.empty()) {
    uncompressed_vertices = GenerateTestPositions(30);
    auto bw = BowyerWatson(uncompressed_vertices);
    uncompressed_indices.assign(bw.begin(), bw.end());
  } else if (fastObjMesh* m = fast_obj_read(obj_path.c_str())) {
    unsigned int offset = 0;
    for (unsigned int i = 0; i < m->face_count;
         offset += m->face_vertices[i++]) {
      for (unsigned int j = 2; j < m->face_vertices[i]; ++j) {
        for (int k : {0, static_cast<int>(j) - 1, static_cast<int>(j)}) {
          auto p = m->indices[offset + k].p * 3;
          uncompressed_vertices.push_back(
              {m->positions[p], m->positions[p + 1], m->positions[p + 2]});
          uncompressed_indices.push_back(
              static_cast<unsigned int>(uncompressed_vertices.size() - 1));
        }
      }
    }
    fast_obj_destroy(m);
  }

  size_t total_indices = uncompressed_indices.size();
  std::vector<unsigned int> remap(total_indices);
  size_t unique_verts = meshopt_generateVertexRemap(
      remap.data(), uncompressed_indices.data(), total_indices,
      uncompressed_vertices.data(), total_indices, sizeof(glm::vec3));

  std::vector<glm::vec3> vertices(unique_verts);
  std::vector<unsigned int> indices(total_indices);
  meshopt_remapVertexBuffer(vertices.data(), uncompressed_vertices.data(),
                            total_indices, sizeof(glm::vec3), remap.data());
  meshopt_remapIndexBuffer(indices.data(), uncompressed_indices.data(),
                           total_indices, remap.data());
  meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(),
                              vertices.size());
  meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(),
                           &vertices[0].x, vertices.size(), sizeof(glm::vec3),
                           1.05f);
  meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(),
                              vertices.data(), vertices.size(),
                              sizeof(glm::vec3));

  struct Part {
    std::vector<int> local_indices;
    std::vector<unsigned int> to_global_v;
  };
  std::vector<Part> parts;

  if (single_mesh) {
    std::vector<unsigned int> identity_v(vertices.size());
    std::iota(identity_v.begin(), identity_v.end(), 0);
    parts.push_back({std::vector<int>(indices.begin(), indices.end()),
                     std::move(identity_v)});
  } else {
    size_t max_m = meshopt_buildMeshletsBound(indices.size(), 64, 124);
    std::vector<meshopt_Meshlet> ml(max_m);
    std::vector<unsigned int> ml_v(max_m * 64);
    std::vector<unsigned char> ml_tri(max_m * 124 * 3);
    ml.resize(meshopt_buildMeshlets(
        ml.data(), ml_v.data(), ml_tri.data(), indices.data(), indices.size(),
        &vertices[0].x, vertices.size(), sizeof(glm::vec3), 64, 124, 0.0f));

    for (const auto& m : ml) {
      parts.push_back({std::vector<int>(ml_tri.begin() + m.triangle_offset,
                                        ml_tri.begin() + m.triangle_offset +
                                            m.triangle_count * 3),
                       std::vector<unsigned int>(
                           ml_v.begin() + m.vertex_offset,
                           ml_v.begin() + m.vertex_offset + m.vertex_count)});
    }
  }

  std::vector<float> dual_graph_positions;
  std::vector<unsigned int> strip_indices;
  std::vector<unsigned int> ordered_indices;
  std::vector<int> strip_states;
  std::vector<int> triangle_states;
  int global_face_offset = 0;
  int global_path_idx = 0;
  int part_id = 0;

  for (const auto& part : parts) {
    DualGraph<int> graph(part.local_indices,
                         static_cast<int>(part.to_global_v.size()));
    auto paths = PathCover<DualGraph<int>, int>(graph).GeneratePaths();

    size_t tri_count = part.local_indices.size() / 3;

    // If single_mesh is true, map each triangle index to its strip ID
    std::vector<int> tri_to_strip;
    if (single_mesh) {
      tri_to_strip.assign(tri_count, global_path_idx);
      int path_id = global_path_idx;
      for (const auto& path : paths) {
        for (int tri_idx : path) {
          tri_to_strip[tri_idx] = path_id;
        }
        path_id++;
      }
    }

    for (size_t i = 0; i < tri_count; ++i) {
      unsigned int v0 = part.to_global_v[part.local_indices[i * 3 + 0]];
      unsigned int v1 = part.to_global_v[part.local_indices[i * 3 + 1]];
      unsigned int v2 = part.to_global_v[part.local_indices[i * 3 + 2]];

      ordered_indices.insert(ordered_indices.end(), {v0, v1, v2});

      int state = single_mesh ? (tri_to_strip[i] + 2) : (part_id + 2);
      triangle_states.insert(triangle_states.end(), 3, state);

      glm::vec3 c = (vertices[v0] + vertices[v1] + vertices[v2]) / 3.0f;
      dual_graph_positions.insert(dual_graph_positions.end(), {c.x, c.y, c.z});
    }

    for (const auto& path : paths) {
      if (path.size() >= 2) {
        for (size_t i = 0; i < path.size() - 1; ++i) {
          strip_indices.insert(
              strip_indices.end(),
              {static_cast<unsigned int>(global_face_offset + path[i]),
               static_cast<unsigned int>(global_face_offset + path[i + 1])});
          strip_states.insert(strip_states.end(), 2, global_path_idx + 2);
        }
      }
      global_path_idx++;
    }
    global_face_offset += static_cast<int>(tri_count);
    part_id++;
  }

  MeshViewer mv;
  mv.UploadMesh(
      reinterpret_cast<const float*>(vertices.data()), ordered_indices.data(),
      static_cast<int>(ordered_indices.size()), triangle_states.data());
  mv.UploadStrips(dual_graph_positions.data(), strip_indices.data(),
                  static_cast<int>(strip_indices.size()), strip_states.data());
  mv.SetWindowTitle(obj_path.empty() ? "ETA" : obj_path.c_str());
  mv.Draw();

  return 0;
}