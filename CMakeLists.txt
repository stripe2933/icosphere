cmake_minimum_required(VERSION 3.12)
project(icosphere)

add_executable(icosphere main.cpp)
target_compile_features(icosphere PRIVATE cxx_std_20)
target_include_directories(icosphere PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/extlibs)

include(FetchContent)
FetchContent_Declare(
    OpenGLApp
    GIT_REPOSITORY https://github.com/stripe2933/OpenGLApp.git
    GIT_TAG c58cad8
)
FetchContent_MakeAvailable(OpenGLApp)

find_package(glm REQUIRED)
find_package(imgui REQUIRED)
target_link_libraries(icosphere PRIVATE glm::glm OpenGLApp imgui::imgui)

# Copy shader files to executable folder.
add_custom_target(copy_shaders COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/copy_shaders.cmake)
add_dependencies(${PROJECT_NAME} copy_shaders)
