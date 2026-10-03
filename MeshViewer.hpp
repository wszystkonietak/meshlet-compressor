#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include "mesh.hpp"

class MeshViewer
{
public:
    MeshViewer() 
    {
        setup();
    }
    void draw()
    {
        while (!glfwWindowShouldClose(window))
        {
            PrimitiveOnlyMesh::draw_call_id = 0;
            glfwPollEvents();
            float currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;
            processInput();

            int w, h;
            glfwGetWindowSize(window, &w, &h);
            glm::mat4 projection = glm::perspective(glm::radians(fov), (float)w / (float)h, 0.1f, 1000.0f);
            glm::mat4 view = glm::lookAt(cameraPos, lookAtPos, cameraUp);

            glClearColor(0.125f, 0.094f, 0.235f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(shader);
            glUniformMatrix4fv(PrimitiveOnlyMesh::projection_location, 1, GL_FALSE, &projection[0][0]);
            glUniformMatrix4fv(PrimitiveOnlyMesh::view_location, 1, GL_FALSE, &view[0][0]);

            graph.draw();
            graph2.draw();
            mesh.draw();
            glfwSwapBuffers(window);
        }

        glDeleteProgram(shader);
        glfwTerminate();
    }
    
    void upload_mesh(float *positions, unsigned int *indices, int index_count)
    {
        mesh.set_mode(GL_TRIANGLES);
        mesh.create_primitive_mesh(positions, indices, index_count);
    }

    void upload_strips(float *positions, unsigned int *indices, int index_count, int *states,
                       float *positions2 = nullptr, unsigned int *indices2 = nullptr, int index_count2 = 0, int *states2 = nullptr)
    {
        graph.set_mode(GL_LINES);
        graph.create_primitive_mesh(positions, indices, index_count, states);
        if(positions2 != nullptr)
        {
            graph2.set_mode(GL_LINES);
            graph2.create_primitive_mesh(positions2, indices2, index_count2, states2);
        }
        
    }

    void set_window_title(const char* title)
    {
        glfwSetWindowTitle(window, title);
    }
private:
    
    void setup()
    {
        if (!glfwInit())
        {
            std::cerr << "ERROR: GLFW Initialization Failed!" << std::endl;
            abort();
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(600, 600, "", nullptr, nullptr);
        if (!window)
        {
            std::cerr << "ERROR: GLFW Window Creation Failed!" << std::endl;
            glfwTerminate();
            abort();
        }
        glfwMakeContextCurrent(window);
        glfwSetWindowAttrib(window, GLFW_FLOATING, GLFW_TRUE);
        glfwSwapInterval(1);
        // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if (glewInit() != GLEW_OK)
        {
            std::cerr << "ERROR: GLEW Initialization Failed!" << std::endl;
            abort();
        }
        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
        glEnable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        shader = createShaderProgram(vertex_shader, fragment_shader);

        // lookAtPos = find_mesh_center(positions, indices, index_count);
        
        PrimitiveOnlyMesh::graph_color_location = glGetUniformLocation(shader, "uId");
        PrimitiveOnlyMesh::projection_location = glGetUniformLocation(shader, "uProjection");
        PrimitiveOnlyMesh::view_location = glGetUniformLocation(shader, "uView");
    }

    glm::vec3 find_mesh_center(float *positions, int *indices, int index_count)
    {
        glm::vec3 center(0.0f);

        for (int i = 0; i < index_count; ++i)
        {
            int idx = indices[i];
            glm::vec3 vertex(
                positions[3 * idx],
                positions[3 * idx + 1],
                positions[3 * idx + 2]);
            center += vertex;
        }

        if (index_count > 0)
            center /= static_cast<float>(index_count);

        return center;
    }

    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 lookAtPos = glm::vec3(0, 0, 0);

    bool firstMouse = true;
    float yaw = -90.0f;
    float pitch = 0.0f;
    float lastX = 800.0f / 2.0;
    float lastY = 600.0 / 2.0;
    float fov = 45.0f;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    GLFWwindow *window;
    GLuint shader;
    PrimitiveOnlyMesh graph, graph2, mesh;

    std::unordered_map<GLenum, bool> key_state;
    bool mesh_needs_update = true;

    float angle_horizontal = 0.1, angle_vertical = 0.1, radius = 1.0;

    glm::vec3 getCameraPosition(float radius, float theta, float phi)
    {
        float x = radius * sin(phi) * cos(theta);
        float y = radius * cos(phi);
        float z = radius * sin(phi) * sin(theta);

        return glm::vec3(x, y, z);
    }

    void processInput()
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        float cameraSpeed = static_cast<float>(2.5 * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            angle_vertical -= cameraSpeed * 2.0;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            angle_vertical += cameraSpeed * 2.0;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            angle_horizontal += cameraSpeed * 2.0;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            angle_horizontal -= cameraSpeed * 2.0;

        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            radius -= (radius)*cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            radius += (radius)*cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
            lookAtPos.y -= radius * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
            lookAtPos.y += radius * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
            lookAtPos.x -= radius * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
            lookAtPos.x += radius * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)
            lookAtPos.z -= radius * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
            lookAtPos.z += radius * cameraSpeed;
        angle_vertical = glm::clamp(angle_vertical, 0.1f, 3.14f);
        cameraPos = getCameraPosition(radius, angle_horizontal, angle_vertical);
    }

    GLuint compileShader(const char *source, GLenum shaderType)
    {
        GLuint shader = glCreateShader(shaderType);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "ERROR: Shader Compilation Failed\n"
                      << infoLog << std::endl;
        }
        return shader;
    }

    GLuint createShaderProgram(const char *vertex, const char *fragment)
    {
        GLuint vertexShader = compileShader(vertex, GL_VERTEX_SHADER);
        GLuint fragmentShader = compileShader(fragment, GL_FRAGMENT_SHADER);

        GLuint shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        GLint success;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            std::cerr << "ERROR: Shader Program Linking Failed\n"
                      << infoLog << std::endl;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return shaderProgram;
    }
    const char *vertex_shader = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in int aState;
    
        out vec3 color;
        flat out int state;
        out vec3 fragPos; 
        flat out vec3 pointPos; 
        uniform int uId;
    
        uniform mat4 uView;
        uniform mat4 uProjection;
    
        // Reverted to uint to prevent fatal signed overflow
        uint hash(uint x) {
            x++;
            x += ( x << 10u );
            x ^= ( x >>  6u );
            x += ( x <<  3u );
            x ^= ( x >> 11u );
            x += ( x << 15u );
            return x;
        }
    
        void main()
        {
            gl_PointSize = 10.0;
            state = aState;
            if(state < 2)
            {
                fragPos = aPos;
                pointPos = aPos;
                uint h = hash(uint(uId));
                float r = float((h >>  0u) & 0xFFu) / 255.0;
                float g = float((h >>  8u) & 0xFFu) / 255.0;
                float b = float((h >> 16u) & 0xFFu) / 255.0;
                color = vec3(r, g, b);
            }
            else 
            {            
                fragPos = aPos;
                pointPos = aPos;
                uint h = hash(uint(10 * uId + state));
                float r = float((h >>  0u) & 0xFFu) / 255.0;
                float g = float((h >>  8u) & 0xFFu) / 255.0;
                float b = float((h >> 16u) & 0xFFu) / 255.0;
                color = vec3(r, g, b);
            }
            gl_Position = uProjection * uView * vec4(aPos, 1.0);
        }
    )";
    
    const char *fragment_shader = R"(
        #version 330 core
    
        in vec3 color;
        in vec3 fragPos;
        flat in vec3 pointPos; 
        flat in int state;
    
        out vec4 FragColor;
    
        void main()
        {
            if (state == 1)
            {
                if (int(distance(pointPos.xy, fragPos.xy) * 25.0) % 2 == 1) 
                    discard; 
            }
            FragColor = vec4(color, 1.0); 
        }
    )";
};