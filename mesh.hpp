#pragma once
#include <GL/glew.h>
#include <vector>
#include <glm/glm.hpp>

class Mesh;
struct Vertex
{
    glm::vec3 pos;
    int state;
};

struct PrimitiveOnlyMesh
{
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
    void set_mode(GLenum m) {
        mode = m;
        stride = mode == GL_TRIANGLES ? 3 : mode == GL_LINES ? 2 : 1;
    }
    void create_primitive_mesh(float *positions, unsigned int *indices, int index_count, int *states = nullptr);
    void create_primitive_mesh(const std::vector<glm::vec3> &positions);
    void update_primitive_indices(const std::vector<glm::vec3> &positions);
    void update_primitive_indices(Mesh &mesh, std::vector<std::pair<int, int>> &primitives);
    void update_primitive_indices(Mesh &mesh, std::vector<int> &primitives);
    void update_primitive_states();
    void draw();

    static int graph_color_location;
    static int projection_location;
    static int view_location;
    static int draw_call_id;
    static int meshes_count;
    std::vector<int> states;
    std::vector<int> indices;
    int stride;
    GLenum mode;
protected:
    void upload_buffer();
    void generate_primitive_data(const std::vector<glm::vec3> &positions);
    void generate_primitive_data(Mesh &mesh, std::vector<std::pair<int, int>> &primitives);
    void generate_primitive_data(Mesh &mesh, std::vector<int> &primitives);

    GLuint VBO2, VAO2;
    int mesh_id;
    std::vector<Vertex> primitive_data;
};

struct Mesh : public PrimitiveOnlyMesh 
{
    Mesh(GLenum m) : PrimitiveOnlyMesh(m) { }
    ~Mesh()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }
    void create_mesh();
    void update_indices();
    void draw_points();

    static int point_color_location;
    static int projection_location;
    static int view_location;
    std::vector<glm::vec3> positions;
private:
    GLuint VBO, VAO;
};

