#include "mesh.hpp"

int PrimitiveOnlyMesh::graph_color_location = 0;
int PrimitiveOnlyMesh::projection_location = 0;
int PrimitiveOnlyMesh::view_location = 0;
int PrimitiveOnlyMesh::draw_call_id = 0;
int PrimitiveOnlyMesh::meshes_count = 0;
int Mesh::point_color_location = 0;
int Mesh::projection_location = 0;
int Mesh::view_location = 0;

void PrimitiveOnlyMesh::create_primitive_mesh(float *positions, unsigned int *indices, int index_count, int *states) 
{
    primitive_data.resize(index_count);
    for (int i = 0; i < index_count; i++)
    {
        primitive_data[i].pos.x = positions[3 * indices[i]];
        primitive_data[i].pos.y = positions[3 * indices[i] + 1];
        primitive_data[i].pos.z = positions[3 * indices[i] + 2];
        primitive_data[i].state = states == nullptr ? 150 : states[i / stride];
    }
    glGenVertexArrays(1, &VAO2);
    glGenBuffers(1, &VBO2);
    glBindVertexArray(VAO2);

    glBindBuffer(GL_ARRAY_BUFFER, VBO2);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * primitive_data.size(), primitive_data.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribIPointer(1, 1, GL_INT, sizeof(Vertex),
                           (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void PrimitiveOnlyMesh::create_primitive_mesh(const std::vector<glm::vec3> &positions)
{
    generate_primitive_data(positions);
    glGenVertexArrays(1, &VAO2);
    glGenBuffers(1, &VBO2);
    glBindVertexArray(VAO2);

    glBindBuffer(GL_ARRAY_BUFFER, VBO2);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * primitive_data.size(), primitive_data.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribIPointer(1, 1, GL_INT, sizeof(Vertex),
                           (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void PrimitiveOnlyMesh::update_primitive_indices(const std::vector<glm::vec3> &positions)
{
    generate_primitive_data(positions);
    upload_buffer();
}

void PrimitiveOnlyMesh::update_primitive_indices(Mesh &mesh, std::vector<std::pair<int, int>> &primitives)
{
    generate_primitive_data(mesh, primitives);
    upload_buffer();
}

void PrimitiveOnlyMesh::update_primitive_indices(Mesh &mesh, std::vector<int> &primitives)
{
    generate_primitive_data(mesh, primitives);
    upload_buffer();
}

void PrimitiveOnlyMesh::draw()
{
    glUniform1i(PrimitiveOnlyMesh::graph_color_location, PrimitiveOnlyMesh::draw_call_id++);
    glBindVertexArray(VAO2);
    glDrawArrays(mode, 0, primitive_data.size());
    glBindVertexArray(0);
}

void PrimitiveOnlyMesh::upload_buffer()
{
    glBindVertexArray(VAO2);
    glBindBuffer(GL_ARRAY_BUFFER, VBO2);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * primitive_data.size(), primitive_data.data(), GL_STATIC_DRAW);
}

void PrimitiveOnlyMesh::update_primitive_states()
{
    for (int i = 0; i < states.size(); i++)
    {
        if(mode == GL_TRIANGLES)
        {
            primitive_data[3 * i + 0].state = 2 + states[i];
            primitive_data[3 * i + 1].state = 2 + states[i];
            primitive_data[3 * i + 2].state = 2 + states[i];
        }   
        else
        {
            primitive_data[2 * i + 0].state = states[i];
            primitive_data[2 * i + 1].state = states[i];
        }
    }
    upload_buffer();
}

void PrimitiveOnlyMesh::generate_primitive_data(const std::vector<glm::vec3> &positions)
{
    primitive_data.resize(indices.size());
    for (int i = 0; i < indices.size(); i++)
    {
        primitive_data[i].pos = positions[indices[i]];
        primitive_data[i].state = states.size() ? mode == GL_TRIANGLES ? 2 + states[i / stride] : states[i / stride] : 0;
    }
}

void PrimitiveOnlyMesh::generate_primitive_data(Mesh &mesh, std::vector<std::pair<int, int>> &primitives)
{
    states.clear();
    indices.clear();
    for (const auto &primitive_data : primitives)
    {
        indices.push_back(mesh.indices[3 * primitive_data.first + 0]);
        indices.push_back(mesh.indices[3 * primitive_data.first + 1]);
        indices.push_back(mesh.indices[3 * primitive_data.first + 2]);
        states.push_back(primitive_data.second | (mesh_id << 16));
    }
    generate_primitive_data(mesh.positions);
}

void PrimitiveOnlyMesh::generate_primitive_data(Mesh &mesh, std::vector<int> &primitives)
{

    states.clear();
    indices.clear();

    for (const auto &primitive_data : primitives)
    {
        indices.push_back(mesh.indices[3 * primitive_data + 0]);
        indices.push_back(mesh.indices[3 * primitive_data + 1]);
        indices.push_back(mesh.indices[3 * primitive_data + 2]);
        states.push_back(mesh_id << 16);
    }
    generate_primitive_data(mesh.positions);
}

void Mesh::create_mesh()
{
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

void Mesh::update_indices()
{
    update_primitive_indices(positions);
}

void Mesh::draw_points()
{
    glUniform1i(Mesh::point_color_location, PrimitiveOnlyMesh::draw_call_id++);
    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, positions.size());
    glBindVertexArray(0);
}