#include <algorithm>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

#include "MeshViewer.hpp"
#include "eta.hpp"
#include "fast_obj.h"
#include "meshoptimizer.h"
#include "utils.hpp"

int main(int argc, char** argv) {
  std::vector<glm::vec3> uncompressed_vertices;
  std::vector<unsigned int> uncompressed_indices;

  if (argc < 2) {
    int n = 30;
    uncompressed_vertices = generate_test_positions(n);
    std::vector<int> bw_indices = bowyer_watson(uncompressed_vertices);
    uncompressed_indices.assign(bw_indices.begin(), bw_indices.end());
  } else {
    const char* obj_path = argv[1];

    fastObjMesh* mesh = fast_obj_read(obj_path);

    unsigned int index_offset = 0;
    for (unsigned int i = 0; i < mesh->face_count; ++i) {
      unsigned int num_verts = mesh->face_vertices[i];

      for (unsigned int j = 2; j < num_verts; ++j) {
        fastObjIndex idx[3] = {mesh->indices[index_offset],
                               mesh->indices[index_offset + j - 1],
                               mesh->indices[index_offset + j]};

        for (int k = 0; k < 3; ++k) {
          glm::vec3 v;
          v.x = mesh->positions[idx[k].p * 3 + 0];
          v.y = mesh->positions[idx[k].p * 3 + 1];
          v.z = mesh->positions[idx[k].p * 3 + 2];

          uncompressed_vertices.push_back(v);
          uncompressed_indices.push_back(uncompressed_vertices.size() - 1);
        }
      }
      index_offset += num_verts;
    }
    fast_obj_destroy(mesh);
  }

  size_t total_indices = uncompressed_indices.size();
  std::vector<unsigned int> remap(total_indices);
  size_t unique_vertices = meshopt_generateVertexRemap(
      remap.data(), uncompressed_indices.data(), total_indices,
      uncompressed_vertices.data(), total_indices, sizeof(glm::vec3));

  std::vector<glm::vec3> vertices(unique_vertices);
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

  const size_t max_vertices = 64;
  const size_t max_triangles = 124;
  const float cone_weight = 0.0f;

  size_t max_meshlets =
      meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);

  std::vector<meshopt_Meshlet> meshlets(max_meshlets);
  std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
  std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles *
                                               3);

  size_t meshlet_count = meshopt_buildMeshlets(
      meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(),
      indices.data(), indices.size(), &vertices[0].x, vertices.size(),
      sizeof(glm::vec3), max_vertices, max_triangles, cone_weight);
  meshlets.resize(meshlet_count);

  std::vector<float> dual_graph_positions;
  std::vector<unsigned int> strip_indices;
  std::vector<int> strip_states;
  
  std::vector<unsigned int> meshlet_ordered_indices;
  std::vector<int> mesh_triangle_states;

  int global_face_offset = 0;
  int global_path_idx = 0;
  int current_meshlet_id = 0;

  for (const auto& meshlet : meshlets) {
    std::vector<int> local_indices(meshlet.triangle_count * 3);
    for (unsigned int i = 0; i < meshlet.triangle_count * 3; ++i) {
      local_indices[i] = meshlet_triangles[meshlet.triangle_offset + i];
    }

    DualGraph<int> graph(local_indices, meshlet.vertex_count);
    PathCover<DualGraph<int>, int> path_cover(graph);
    std::vector<std::vector<int>> generated_paths = path_cover.GeneratePaths();

    for (unsigned int i = 0; i < meshlet.triangle_count; ++i) {
      unsigned int v0_local =
          meshlet_triangles[meshlet.triangle_offset + i * 3 + 0];
      unsigned int v1_local =
          meshlet_triangles[meshlet.triangle_offset + i * 3 + 1];
      unsigned int v2_local =
          meshlet_triangles[meshlet.triangle_offset + i * 3 + 2];

      unsigned int v0_global =
          meshlet_vertices[meshlet.vertex_offset + v0_local];
      unsigned int v1_global =
          meshlet_vertices[meshlet.vertex_offset + v1_local];
      unsigned int v2_global =
          meshlet_vertices[meshlet.vertex_offset + v2_local];

      glm::vec3 centroid =
          (vertices[v0_global] + vertices[v1_global] + vertices[v2_global]) /
          3.0f;
          
      // Push the unrolled indices for unique per-triangle coloring
      meshlet_ordered_indices.push_back(v0_global);
      meshlet_ordered_indices.push_back(v1_global);
      meshlet_ordered_indices.push_back(v2_global);
      
      // Push 3 identical states per triangle (+2 offset for the shader uniqueness check)
      mesh_triangle_states.push_back(current_meshlet_id + 2);
      mesh_triangle_states.push_back(current_meshlet_id + 2);
      mesh_triangle_states.push_back(current_meshlet_id + 2);

      dual_graph_positions.push_back(centroid.x);
      dual_graph_positions.push_back(centroid.y);
      dual_graph_positions.push_back(centroid.z);
    }

    for (const auto& path : generated_paths) {
      if (path.size() < 2) continue;

      for (size_t i = 0; i < path.size() - 1; ++i) {
        strip_indices.push_back(global_face_offset + path[i]);
        strip_indices.push_back(global_face_offset + path[i + 1]);

        // Push 2 identical states per line segment (+2 offset for shader uniqueness check)
        strip_states.push_back(global_path_idx + 2);
        strip_states.push_back(global_path_idx + 2);
      }
      global_path_idx++;
    }

    global_face_offset += meshlet.triangle_count;
    current_meshlet_id++;
  }

  std::vector<float> mesh_positions(vertices.size() * 3);
  for (size_t i = 0; i < vertices.size(); ++i) {
    mesh_positions[i * 3 + 0] = vertices[i].x;
    mesh_positions[i * 3 + 1] = vertices[i].y;
    mesh_positions[i * 3 + 2] = vertices[i].z;
  }

  MeshViewer mv;
  // Upload with the newly populated index/state vectors
  mv.upload_mesh(mesh_positions.data(), meshlet_ordered_indices.data(), 
                 meshlet_ordered_indices.size(), mesh_triangle_states.data());
  mv.upload_strips(dual_graph_positions.data(), strip_indices.data(),
                   strip_indices.size(), strip_states.data());
  mv.set_window_title(argc < 2 ? "ETA" : argv[1]);
  mv.draw();

  return 0;
}