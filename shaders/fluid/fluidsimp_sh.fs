#version 460 core
in vec3 vVelocity;
in vec3 bPos;
out vec4 FragColor;

float max_vel_display = 8.0;

vec3 color1 = vec3(34,87,185)/255;
vec3 color2 = vec3(76,255,144)/255;
vec3 color3 = vec3(255,237,0)/255;
vec3 color4 = vec3(234,73,8)/255;

uniform bool render_vel_field = true;
uniform bool render_as_circle = true;
uniform vec3 fluid_color = vec3(34,87,185)/255;

void main()
{   
    //vec3 color = vec3(0.0);

    if(render_as_circle)
        if(dot(bPos,bPos) > 0.25) discard;

    vec3 color = fluid_color;

    if(render_vel_field){
        float velocity = length(vVelocity);
        float t = max(0.0,min(velocity/max_vel_display,1.0));

        if(0 <= t && t <0.062) color = color1;
        if(0.062 <= t && t <0.506) color = mix(color1,color2,(t-0.062)/(0.506-0.062));
        if(0.506 <= t && t <0.713) color = mix(color2,color3,(t-0.506)/(0.713-0.506));
        if(0.713 <= t && t <= 1) color = mix(color3,color4,(t-0.713)/(1.0-0.713));
    }

    FragColor = vec4(color, 1.0);
    //FragColor = vec4(0.0,0.0,1.0, 1.0);
}