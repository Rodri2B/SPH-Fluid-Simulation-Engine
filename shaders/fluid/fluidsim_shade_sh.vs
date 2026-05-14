#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aVel;
layout(location = 2) in float aDensity;
layout(location = 3) in float aBoundary;

out vec3 vVelocity;
out vec3 center_pos;
out vec3 bPos;
out float Radius;
//out vec2 uv;

/////////////////////]

uniform Matrices_cam
{
    mat4 view;
    mat4 projection;
};

//////////////////////]

uniform float square_size = 0.08;

const vec4 Pos[4] = vec4[](
    vec4(-0.5, -0.5, 0.0, 0.0),      // bottom left
    vec4( 0.5, -0.5, 0.0, 0.0),      // bottom right
    vec4( 0.5,  0.5, 0.0, 0.0),      // top right
    vec4(-0.5,  0.5, 0.0, 0.0)       // top left
);

//vec2 Tex_Coord[4] = vec2[](
//    vec2( 0.0, 0.0),      // bottom left
//    vec2( 1.0, 0.0),      // bottom right
//    vec2( 1.0, 1.0),      // top right
//    vec2( 0.0, 1.0)       // top left
//);

const int Indices[6] = int[](0, 2, 3, 2, 0, 1);

void main() {

    vVelocity = aVel;

    Radius = square_size/2.0;

    vec4 center_position =  view*vec4(aPos,1.0);

    center_pos = vec3(center_position);

    int Index = Indices[gl_VertexID];
    vec4 vertex_pos = center_position+square_size*Pos[Index];
    bPos = vec3(vertex_pos);
    //vec4 t_pos = projection*vertex_pos;
    //uv = 0.5*((t_pos.xy)/t_pos.w)+vec2(0.5);
    //gl_Position = t_pos;
    gl_Position = projection*vertex_pos;
}