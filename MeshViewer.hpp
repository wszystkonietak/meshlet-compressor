#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

#include "Mesh.hpp"

inline constexpr const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in int aState;

out vec3 color;
out vec3 vColor;

uniform int uId;
uniform mat4 uView;
uniform mat4 uProjection;

vec3 Hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 ColorFromID(int id, bool is_strip) {
    float hue = fract(float(2*id + int(is_strip)) * 0.6180339887);
    float saturation = is_strip ? 0.95 : 0.75;
    float brightness = is_strip ? 1.00 : 0.55;
    return Hsv2rgb(vec3(hue, saturation, brightness));
}

void main() {
    gl_PointSize = 10.0;
    bool is_strip = (uId < 2);
    vec3 c = ColorFromID(aState, is_strip);
    color = c;
    vColor = c;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)";

inline constexpr const char* kFragmentShader = R"(
#version 330 core
in vec3 color;
out vec4 FragColor;

void main() {
    FragColor = vec4(color, 1.0);
}
)";

inline constexpr const char* kLineGeometryShader = R"(
#version 330 core
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec3 vColor[];
out vec3 color;

uniform vec2 uViewport;
uniform float uLineWidth;

void main() {
    vec2 p0 = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
    vec2 p1 = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;

    vec2 diff = (p1 - p0) * uViewport;
    float len = length(diff);
    if (len < 0.0001) return;

    vec2 dir = diff / len;
    vec2 normal = vec2(-dir.y, dir.x) * (uLineWidth * 0.5 / uViewport);

    vec4 offset0 = vec4(normal * gl_in[0].gl_Position.w, 0.0, 0.0);
    vec4 offset1 = vec4(normal * gl_in[1].gl_Position.w, 0.0, 0.0);

    color = vColor[0];
    gl_Position = gl_in[0].gl_Position + offset0;
    EmitVertex();

    gl_Position = gl_in[0].gl_Position - offset0;
    EmitVertex();

    color = vColor[1];
    gl_Position = gl_in[1].gl_Position + offset1;
    EmitVertex();

    gl_Position = gl_in[1].gl_Position - offset1;
    EmitVertex();

    EndPrimitive();
}
)";

inline constexpr const char* kWireframeGeometryShader = R"(
#version 330 core
layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out;

in vec3 vColor[];
out vec3 color;

uniform vec2 uViewport;
uniform float uLineWidth;

void emitEdge(vec4 posA, vec4 posB, vec3 colA, vec3 colB) {
    vec2 p0 = posA.xy / posA.w;
    vec2 p1 = posB.xy / posB.w;

    vec2 diff = (p1 - p0) * uViewport;
    float len = length(diff);
    if (len < 0.0001) return;

    vec2 dir = diff / len;
    vec2 normal = vec2(-dir.y, dir.x) * (uLineWidth * 0.5 / uViewport);

    vec4 offset0 = vec4(normal * posA.w, 0.0, 0.0);
    vec4 offset1 = vec4(normal * posB.w, 0.0, 0.0);

    color = colA;
    gl_Position = posA + offset0;
    EmitVertex();

    gl_Position = posA - offset0;
    EmitVertex();

    color = colB;
    gl_Position = posB + offset1;
    EmitVertex();

    gl_Position = posB - offset1;
    EmitVertex();

    EndPrimitive();
}

void main() {
    emitEdge(gl_in[0].gl_Position, gl_in[1].gl_Position, vColor[0], vColor[1]);
    emitEdge(gl_in[1].gl_Position, gl_in[2].gl_Position, vColor[1], vColor[2]);
    emitEdge(gl_in[2].gl_Position, gl_in[0].gl_Position, vColor[2], vColor[0]);
}
)";

struct ShaderProgram {
  GLuint id = 0;
  GLint u_view = -1;
  GLint u_projection = -1;
  GLint u_id = -1;
  GLint u_viewport = -1;
  GLint u_line_width = -1;

  ShaderProgram() = default;

  ~ShaderProgram() {
    if (id != 0) {
      glDeleteProgram(id);
    }
  }

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  ShaderProgram(ShaderProgram&& other) noexcept
      : id(std::exchange(other.id, 0)),
        u_view(other.u_view),
        u_projection(other.u_projection),
        u_id(other.u_id),
        u_viewport(other.u_viewport),
        u_line_width(other.u_line_width) {}

  ShaderProgram& operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
      if (id != 0) {
        glDeleteProgram(id);
      }
      id = std::exchange(other.id, 0);
      u_view = other.u_view;
      u_projection = other.u_projection;
      u_id = other.u_id;
      u_viewport = other.u_viewport;
      u_line_width = other.u_line_width;
    }
    return *this;
  }
};

struct GlfwWindowDeleter {
  void operator()(GLFWwindow* window) const {
    if (window) {
      glfwDestroyWindow(window);
    }
  }
};

class MeshViewer {
 public:
  MeshViewer() { Setup(); }

  ~MeshViewer() {
    window_.reset();
    glfwTerminate();
  }

  void ComputeMeshCenter(const float* positions, const unsigned int* indices,
                         int index_count) {
    if (!positions || !indices || index_count <= 0) return;

    glm::vec3 min_pos(std::numeric_limits<float>::max());
    glm::vec3 max_pos(-std::numeric_limits<float>::max());

    for (int i = 0; i < index_count; ++i) {
      unsigned int idx = indices[i];
      glm::vec3 p(positions[3 * idx + 0], positions[3 * idx + 1],
                  positions[3 * idx + 2]);
      min_pos = glm::min(min_pos, p);
      max_pos = glm::max(max_pos, p);
    }

    look_at_pos_ = (min_pos + max_pos) * 0.5f;

    float diag = glm::length(max_pos - min_pos);
    if (diag > 0.0001f) {
      radius_ = diag * 1.5f;
    }
    camera_pos_ =
        look_at_pos_ +
        CalculateCameraPosition(radius_, angle_horizontal_, angle_vertical_);
  }

  void UploadMesh(const float* positions, const unsigned int* indices,
                  int index_count, const int* states = nullptr) {
    mesh_.SetMode(GL_TRIANGLES);
    mesh_.CreatePrimitiveMesh(positions, indices, index_count, states);
    ComputeMeshCenter(positions, indices, index_count);
  }

  void UploadStrips(const float* positions, const unsigned int* indices,
                    int index_count, const int* states = nullptr) {
    graph_.SetMode(GL_LINES);
    graph_.CreatePrimitiveMesh(positions, indices, index_count, states);
  }

  void SetWindowTitle(const char* title) {
    if (window_) {
      glfwSetWindowTitle(window_.get(), title);
    }
  }

  void SetLineWidth(float width) { line_width_ = width; }

  void Draw() {
    while (!glfwWindowShouldClose(window_.get())) {
      glfwPollEvents();
      float current_frame = static_cast<float>(glfwGetTime());
      delta_time_ = current_frame - last_frame_;
      last_frame_ = current_frame;
      ProcessInput();

      int w, h;
      glfwGetFramebufferSize(window_.get(), &w, &h);
      if (h == 0) h = 1;

      glm::mat4 projection = glm::perspective(
          glm::radians(fov_), static_cast<float>(w) / static_cast<float>(h),
          0.1f, 1000.0f);
      glm::mat4 view = glm::lookAt(camera_pos_, look_at_pos_, camera_up_);

      glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glUseProgram(basic_shader_.id);
      glUniformMatrix4fv(basic_shader_.u_projection, 1, GL_FALSE,
                         glm::value_ptr(projection));
      glUniformMatrix4fv(basic_shader_.u_view, 1, GL_FALSE,
                         glm::value_ptr(view));
      glUniform1i(basic_shader_.u_id, 2);

      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      if (wireframe_mode_) {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(20.0f, 20.0f);
        mesh_.Draw();
        glDisable(GL_POLYGON_OFFSET_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      } else {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(20.0f, 20.0f);
        mesh_.Draw();
        glDisable(GL_POLYGON_OFFSET_FILL);
      }

      glUseProgram(line_shader_.id);
      glUniformMatrix4fv(line_shader_.u_projection, 1, GL_FALSE,
                         glm::value_ptr(projection));
      glUniformMatrix4fv(line_shader_.u_view, 1, GL_FALSE,
                         glm::value_ptr(view));
      glUniform2f(line_shader_.u_viewport, static_cast<float>(w),
                  static_cast<float>(h));
      glUniform1f(line_shader_.u_line_width, line_width_);
      glUniform1i(line_shader_.u_id, 0);

      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(-30.0f, -30.0f);
      graph_.Draw();
      glDisable(GL_POLYGON_OFFSET_FILL);

      if (wireframe_mode_) {
        glUseProgram(wireframe_shader_.id);
        glUniformMatrix4fv(wireframe_shader_.u_projection, 1, GL_FALSE,
                           glm::value_ptr(projection));
        glUniformMatrix4fv(wireframe_shader_.u_view, 1, GL_FALSE,
                           glm::value_ptr(view));
        glUniform2f(wireframe_shader_.u_viewport, static_cast<float>(w),
                    static_cast<float>(h));
        glUniform1f(wireframe_shader_.u_line_width, line_width_);
        glUniform1i(wireframe_shader_.u_id, 2);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(10.0f, 10.0f);
        mesh_.Draw();
        glDisable(GL_POLYGON_OFFSET_FILL);
      }

      glfwSwapBuffers(window_.get());
    }
  }

 private:
  void Setup() {
    if (!glfwInit()) {
      std::cerr << "ERROR: GLFW Initialization Failed!\n";
      std::abort();
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 8);

    window_.reset(glfwCreateWindow(800, 800, "", nullptr, nullptr));
    if (!window_) {
      std::cerr << "ERROR: GLFW Window Creation Failed!\n";
      glfwTerminate();
      std::abort();
    }
    glfwMakeContextCurrent(window_.get());
    glfwSetWindowAttrib(window_.get(), GLFW_FLOATING, GLFW_TRUE);
    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK) {
      std::cerr << "ERROR: GLEW Initialization Failed!\n";
      std::abort();
    }

    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    basic_shader_ = BuildShaderProgram(kVertexShader, kFragmentShader);
    line_shader_ =
        BuildShaderProgram(kVertexShader, kFragmentShader, kLineGeometryShader);
    wireframe_shader_ = BuildShaderProgram(kVertexShader, kFragmentShader,
                                           kWireframeGeometryShader);

    camera_pos_ =
        look_at_pos_ +
        CalculateCameraPosition(radius_, angle_horizontal_, angle_vertical_);
  }

  static GLuint CompileStage(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      char log[512];
      glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
      std::cerr << "ERROR: Shader Compilation Failed:\n" << log << std::endl;
    }
    return shader;
  }

  static ShaderProgram BuildShaderProgram(const char* vs_src,
                                          const char* fs_src,
                                          const char* gs_src = nullptr) {
    GLuint vs = CompileStage(GL_VERTEX_SHADER, vs_src);
    GLuint fs = CompileStage(GL_FRAGMENT_SHADER, fs_src);
    GLuint gs = gs_src ? CompileStage(GL_GEOMETRY_SHADER, gs_src) : 0;

    ShaderProgram prog;
    prog.id = glCreateProgram();
    glAttachShader(prog.id, vs);
    glAttachShader(prog.id, fs);
    if (gs) glAttachShader(prog.id, gs);
    glLinkProgram(prog.id);

    GLint success;
    glGetProgramiv(prog.id, GL_LINK_STATUS, &success);
    if (!success) {
      char log[512];
      glGetProgramInfoLog(prog.id, sizeof(log), nullptr, log);
      std::cerr << "ERROR: Shader Program Linking Failed:\n"
                << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    if (gs) glDeleteShader(gs);

    prog.u_view = glGetUniformLocation(prog.id, "uView");
    prog.u_projection = glGetUniformLocation(prog.id, "uProjection");
    prog.u_id = glGetUniformLocation(prog.id, "uId");
    prog.u_viewport = glGetUniformLocation(prog.id, "uViewport");
    prog.u_line_width = glGetUniformLocation(prog.id, "uLineWidth");

    return prog;
  }

  static glm::vec3 CalculateCameraPosition(float r, float theta, float phi) {
    return glm::vec3(r * std::sin(phi) * std::cos(theta), r * std::cos(phi),
                     r * std::sin(phi) * std::sin(theta));
  }

  void ProcessInput() {
    if (glfwGetKey(window_.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window_.get(), true);

    bool p_pressed = (glfwGetKey(window_.get(), GLFW_KEY_P) == GLFW_PRESS);
    if (p_pressed && !p_key_was_pressed_) {
      wireframe_mode_ = !wireframe_mode_;
    }
    p_key_was_pressed_ = p_pressed;

    float line_width_speed = 15.0f * delta_time_;
    if (glfwGetKey(window_.get(), GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
      line_width_ += line_width_speed;
    }
    if (glfwGetKey(window_.get(), GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
      line_width_ -= line_width_speed;
    }
    line_width_ = glm::clamp(line_width_, 0.5f, 64.0f);

    float camera_speed = static_cast<float>(2.5 * delta_time_);
    if (glfwGetKey(window_.get(), GLFW_KEY_W) == GLFW_PRESS)
      angle_vertical_ -= camera_speed * 2.0f;
    if (glfwGetKey(window_.get(), GLFW_KEY_S) == GLFW_PRESS)
      angle_vertical_ += camera_speed * 2.0f;
    if (glfwGetKey(window_.get(), GLFW_KEY_A) == GLFW_PRESS)
      angle_horizontal_ += camera_speed * 2.0f;
    if (glfwGetKey(window_.get(), GLFW_KEY_D) == GLFW_PRESS)
      angle_horizontal_ -= camera_speed * 2.0f;
    camera_speed /= 2.5;
    if (glfwGetKey(window_.get(), GLFW_KEY_Q) == GLFW_PRESS)
      radius_ -= radius_ * camera_speed;
    if (glfwGetKey(window_.get(), GLFW_KEY_E) == GLFW_PRESS)
      radius_ += radius_ * camera_speed;
    if (glfwGetKey(window_.get(), GLFW_KEY_R) == GLFW_PRESS)
      look_at_pos_.y -= radius_ * camera_speed;
    if (glfwGetKey(window_.get(), GLFW_KEY_F) == GLFW_PRESS)
      look_at_pos_.y += radius_ * camera_speed;
    if (glfwGetKey(window_.get(), GLFW_KEY_T) == GLFW_PRESS)
      look_at_pos_.x -= radius_ * camera_speed;
    if (glfwGetKey(window_.get(), GLFW_KEY_G) == GLFW_PRESS)
      look_at_pos_.x += radius_ * camera_speed;
    if (glfwGetKey(window_.get(), GLFW_KEY_Y) == GLFW_PRESS)
      look_at_pos_.z -= radius_ * camera_speed;
    if (glfwGetKey(window_.get(), GLFW_KEY_H) == GLFW_PRESS)
      look_at_pos_.z += radius_ * camera_speed;

    angle_vertical_ = glm::clamp(angle_vertical_, 0.1f, 3.14f);
    camera_pos_ =
        look_at_pos_ +
        CalculateCameraPosition(radius_, angle_horizontal_, angle_vertical_);
  }

  std::unique_ptr<GLFWwindow, GlfwWindowDeleter> window_;

  glm::vec3 camera_pos_ = glm::vec3(0.0f, 0.0f, 3.0f);
  glm::vec3 camera_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 look_at_pos_ = glm::vec3(0.0f, 0.0f, 0.0f);

  float fov_ = 45.0f;
  float line_width_ = 12.0f;
  float delta_time_ = 0.0f;
  float last_frame_ = 0.0f;
  float angle_horizontal_ = 0.1f;
  float angle_vertical_ = 0.1f;
  float radius_ = 1.0f;

  bool wireframe_mode_ = true;
  bool p_key_was_pressed_ = false;

  ShaderProgram basic_shader_;
  ShaderProgram line_shader_;
  ShaderProgram wireframe_shader_;

  Mesh graph_;
  Mesh mesh_;
};