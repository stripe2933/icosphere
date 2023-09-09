#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 fragPos;
out vec3 normal;

layout (std140) uniform MvpMatrix{
    mat4 model;
    mat4 inv_model;
    mat4 projection_view;
};

void main(){
    fragPos = aPos;
    normal = aNormal;
    gl_Position = projection_view * vec4(aPos, 1.0);
}