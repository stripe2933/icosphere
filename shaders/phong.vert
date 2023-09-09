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
    fragPos = vec3(model * vec4(aPos, 1.0));
    normal = normalize(mat3(transpose(inv_model)) * aNormal);
    gl_Position = projection_view * vec4(fragPos, 1.0);
}