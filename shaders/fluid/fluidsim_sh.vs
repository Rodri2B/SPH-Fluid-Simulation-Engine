#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aVel;
layout(location = 2) in float aDensity;
layout(location = 3) in float aBoundary;

out vec3 vVelocity;


/////////////////////]

uniform Matrices_cam
{
    mat4 view;
    mat4 projection;
};

//////////////////////]

void main()
{
    vVelocity = aVel;
    gl_Position = projection*view*vec4(aPos, 1.0);
    if(aBoundary < 0.5){
        gl_PointSize = 3.0;  // Set point size here
        gl_Position = projection*view*vec4(aPos, 1.0);
    }else{
        gl_PointSize = 0.0;
        gl_Position  = vec4(2.0, 2.0, 2.0, 1.0); // outside clip space
    }
}