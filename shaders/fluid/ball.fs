#version 460 core
in vec3 vNormal;
out vec4 FragColor;

uniform vec3 color = vec3(1.0,0.0,0.0);

void main()
{   

    FragColor = vec4(color, 1.0);
}