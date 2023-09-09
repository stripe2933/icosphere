//
// Created by gomkyung2 on 2023/09/09.
//

#pragma once

#include <glm/ext/vector_float3.hpp>

struct Vertex{
    glm::vec3 position;
    glm::vec3 normal;
};
static_assert(std::is_standard_layout_v<Vertex>);