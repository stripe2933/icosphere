#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 fragPos;

layout (std140) uniform MvpMatrix{
    mat4 model;
    mat4 inv_model;
    mat4 projection_view;
};

void main(){
    fragPos = aPos;
    gl_Position = projection_view * vec4(aPos, 1.0);
}