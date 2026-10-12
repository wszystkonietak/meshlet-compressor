#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <utility>
#include <vector>

struct Vertex {
  glm::vec3 pos;
  int state;
};

class Mesh {
 public:
  Mesh() = default;
  explicit Mesh(GLenum mode) : mode_(mode) {}

  ~Mesh() { Cleanup(); }

  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  Mesh(Mesh&& other) noexcept
      : vao_(std::exchange(other.vao_, 0)),
        vbo_(std::exchange(other.vbo_, 0)),
        vertex_count_(std::exchange(other.vertex_count_, 0)),
        mode_(other.mode_) {}

  Mesh& operator=(Mesh&& other) noexcept {
    if (this != &other) {
      Cleanup();
      vao_ = std::exchange(other.vao_, 0);
      vbo_ = std::exchange(other.vbo_, 0);
      vertex_count_ = std::exchange(other.vertex_count_, 0);
      mode_ = other.mode_;
    }
    return *this;
  }

  void SetMode(GLenum mode) { mode_ = mode; }

  void CreatePrimitiveMesh(const float* positions, const unsigned int* indices,
                           int index_count, const int* states = nullptr) {
    if (index_count <= 0) return;

    std::vector<Vertex> vertices(index_count);
    for (int i = 0; i < index_count; ++i) {
      unsigned int idx = indices[i];
      vertices[i].pos = glm::vec3(positions[3 * idx + 0], positions[3 * idx + 1],
                                  positions[3 * idx + 2]);
      vertices[i].state = states ? states[i] : 150;
    }

    if (!vao_) glGenVertexArrays(1, &vao_);
    if (!vbo_) glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(),
                 vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(0);

    glVertexAttribIPointer(
        1, 1, GL_INT, sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, state)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    vertex_count_ = static_cast<GLsizei>(index_count);
  }

  void Draw() const {
    if (vao_ && vertex_count_ > 0) {
      glBindVertexArray(vao_);
      glDrawArrays(mode_, 0, vertex_count_);
      glBindVertexArray(0);
    }
  }

 private:
  void Cleanup() {
    if (vao_) {
      glDeleteVertexArrays(1, &vao_);
      vao_ = 0;
    }
    if (vbo_) {
      glDeleteBuffers(1, &vbo_);
      vbo_ = 0;
    }
    vertex_count_ = 0;
  }

  GLuint vao_ = 0;
  GLuint vbo_ = 0;
  GLsizei vertex_count_ = 0;
  GLenum mode_ = GL_TRIANGLES;
};