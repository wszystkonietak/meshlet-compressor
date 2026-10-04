#pragma once
#include <GL/glew.h>
#include <vector>
#include <glm/glm.hpp>
#include <utility>
#include <cstddef>

class Mesh;
struct Vertex
{
    glm::vec3 pos;
    int state;
};

struct PrimitiveOnlyMesh
{
    inline static int graph_color_location = 0;
    inline static int projection_location = 0;
    inline static int view_location = 0;
    inline static int draw_call_id = 0;
    inline static int meshes_count = 0;

    PrimitiveOnlyMesh()
    {
        mesh_id = meshes_count;
        meshes_count++;        
    }
    PrimitiveOnlyMesh(GLenum m) : mode(m) {
        stride = mode == GL_TRIANGLES ? 3 : mode == GL_LINES ? 2 : 1;
        mesh_id = meshes_count;
        meshes_count++;        
    }
    ~PrimitiveOnlyMesh()
    {
        glDeleteVertexArrays(1, &VAO2);
        glDeleteBuffers(1, &VBO2);
    }
    
    inline void set_mode(GLenum m) {
        mode = m;
        stride = mode == GL_TRIANGLES ? 3 : mode == GL_LINES ? 2 : 1;
    }
    
    inline void create_primitive_mesh(float *positions, unsigned int *indices, int index_count, int *states = nullptr) {
        primitive_data.resize(index_count);
        for (int i = 0; i < index_count; i++)
        {
            primitive_data[i].pos.x = positions[3 * indices[i]];
            primitive_data[i].pos.y = positions[3 * indices[i] + 1];
            primitive_data[i].pos.z = positions[3 * indices[i] + 2];
            // Strict 1:1 mapping - expects one state per index passed
            primitive_data[i].state = states == nullptr ? 150 : states[i];
        }
        glGenVertexArrays(1, &VAO2);
        glGenBuffers(1, &VBO2);
        glBindVertexArray(VAO2);

        glBindBuffer(GL_ARRAY_BUFFER, VBO2);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * primitive_data.size(), primitive_data.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
        glEnableVertexAttribArray(0);

        // Safe offset reading to avoid padding issues
        glVertexAttribIPointer(1, 1, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, state));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    inline void create_primitive_mesh(const std::vector<glm::vec3> &positions) {
        generate_primitive_data(positions);
        glGenVertexArrays(1, &VAO2);
        glGenBuffers(1, &VBO2);
        glBindVertexArray(VAO2);

        glBindBuffer(GL_ARRAY_BUFFER, VBO2);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * primitive_data.size(), primitive_data.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
        glEnableVertexAttribArray(0);

        glVertexAttribIPointer(1, 1, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, state));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    inline void update_primitive_indices(const std::vector<glm::vec3> &positions) {
        generate_primitive_data(positions);
        upload_buffer();
    }

    inline void update_primitive_states() {
        for (int i = 0; i < states.size(); i++)
        {
            primitive_data[i].state = states[i];
        }
        upload_buffer();
    }

    inline void draw() {
        glUniform1i(PrimitiveOnlyMesh::graph_color_location, PrimitiveOnlyMesh::draw_call_id++);
        glBindVertexArray(VAO2);
        glDrawArrays(mode, 0, primitive_data.size());
        glBindVertexArray(0);
    }

    std::vector<int> states;
    std::vector<int> indices;
    int stride;
    GLenum mode;
    
protected:
    inline void upload_buffer() {
        glBindVertexArray(VAO2);
        glBindBuffer(GL_ARRAY_BUFFER, VBO2);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * primitive_data.size(), primitive_data.data(), GL_STATIC_DRAW);
    }

    inline void generate_primitive_data(const std::vector<glm::vec3> &positions) {
        primitive_data.resize(indices.size());
        for (int i = 0; i < indices.size(); i++)
        {
            primitive_data[i].pos = positions[indices[i]];
            primitive_data[i].state = states.size() ? states[i] : 0;
        }
    }

    GLuint VBO2, VAO2;
    int mesh_id;
    std::vector<Vertex> primitive_data;
};

struct Mesh : public PrimitiveOnlyMesh 
{
    inline static int point_color_location = 0;
    inline static int projection_location = 0;
    inline static int view_location = 0;

    Mesh(GLenum m) : PrimitiveOnlyMesh(m) { }
    ~Mesh()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }
    
    inline void create_mesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * positions.size(), positions.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);

        create_primitive_mesh(positions);
    }

    inline void update_indices() {
        update_primitive_indices(positions);
    }

    inline void draw_points() {
        glUniform1i(Mesh::point_color_location, PrimitiveOnlyMesh::draw_call_id++);
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, positions.size());
        glBindVertexArray(0);
    }

    std::vector<glm::vec3> positions;
private:
    GLuint VBO, VAO;
};