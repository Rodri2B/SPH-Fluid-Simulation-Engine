#version 460 core
out vec4 FragColor;

uniform bool selected = false;

void main()
{
    if(selected) FragColor = FragColor = vec4(0.96, 0.61, 0.22, 1.0);
    else FragColor = vec4(0.3,0.3,0.3,1.0);
}