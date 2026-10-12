cmake_minimum_required(VERSION 3.10)
project(meshlet_compressor CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLFW3 REQUIRED)

add_library(fast_obj STATIC extern/fast_obj/fast_obj.c)
target_include_directories(fast_obj PUBLIC extern/fast_obj)

add_subdirectory(extern/meshoptimizer)

add_executable(meshlet_compressor
    main.cpp
)

target_include_directories(meshlet_compressor PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    /usr/local/include
)

target_link_libraries(meshlet_compressor PRIVATE 
    fast_obj 
    meshoptimizer
    OpenGL::GL 
    GLEW::GLEW 
    glfw
)