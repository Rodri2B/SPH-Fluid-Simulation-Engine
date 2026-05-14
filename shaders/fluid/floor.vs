#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 vNormal;
out vec2 fPos;

uniform mat4 model;

uniform Matrices_cam
{
    mat4 view;
    mat4 projection;
};

void main()
{
    vNormal = aNormal;
    fPos = aPos.zx; 
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
