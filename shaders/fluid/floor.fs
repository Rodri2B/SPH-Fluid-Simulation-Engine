#version 460 core
out vec4 FragColor;

in vec3 vNormal;
in vec2 fPos;

uniform vec2 floor_dimension = vec2(50.0,50.0);


uniform vec3 tileCol1 = vec3(0.204,0.318,0.776);
uniform vec3 tileCol2 = vec3(0.608,0.369,0.858);
uniform vec3 tileCol3 = vec3(0.302,0.736,0.458);
uniform vec3 tileCol4 = vec3(0.802,0.643,0.367);
uniform vec3 tileColVariation = vec3(0.33,0,0.47);
uniform float tileScale = 1;
uniform float tileDarkOffset = -0.35;

vec3 RGBToHSV(vec3 rgb);
vec3 HSVToRGB(vec3 rgb);
vec3 TweakHSV(vec3 colRGB, vec3 shift); //
float Modulo(float x, float y); //
uint HashInt2(ivec2 v); //
uint NextRandom(uint state);
float RandomValue(uint state);
vec3 RandomSNorm3(uint state); //

void main()
{    
    
    // Choose tileCol based on quadrant
    vec2 frag_pos = fPos*floor_dimension;
    vec3 tileCol = frag_pos.x < 0 ? tileCol1 : tileCol2;
    if (frag_pos.y < 0) tileCol = frag_pos.x < 0 ? tileCol3 : tileCol4;

    // If tile is a dark tile, then darken it
    ivec2 tileCoord;
    tileCoord.x = int(floor(frag_pos.x * tileScale));
    tileCoord.y = int(floor(frag_pos.y * tileScale));
    bool isDarkTile = Modulo(tileCoord.x, 2) == Modulo(tileCoord.y, 2);
    tileCol = TweakHSV(tileCol, vec3(0.0, 0.0, (isDarkTile)?tileDarkOffset:0.0));

    // Vary hue/sat/val randomly
    uint rngState = HashInt2(tileCoord);
    vec3 randomVariation = RandomSNorm3(rngState) * tileColVariation * 0.1;
    tileCol = TweakHSV(tileCol, randomVariation);

    //vec3 shadowMap = Transmittance(CalculateDensityAlongRay(floorInfo.hitPoint, _WorldSpaceLightPos0, lightStepSize * 2) * 2);
    //bool inShadow = RayBoxWithMatrix(floorInfo.hitPoint, dirToSun, cubeLocalToWorld, cubeWorldToLocal).didHit;
    //if (inShadow) shadowMap *= 0.2;
    //return tileCol * shadowMap;

    FragColor = vec4(tileCol,1.0);
}


vec3 TweakHSV(vec3 colRGB, vec3 shift)
{
    vec3 hsv = RGBToHSV(colRGB);
    return clamp(HSVToRGB(hsv + shift), 0.0, 1.0);
}

float Modulo(float x, float y)
{
    return (x - y * floor(x / y));
}

uint HashInt2(ivec2 v)
{
    return v.x * 5023 + v.y * 96456;
}


uint NextRandom(uint state)
{
    state = state * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    result = (result >> 22) ^ result;
    return result;
}
float RandomValue(uint state)
{
    return NextRandom(state) / 4294967295.0; // 2^32 - 1
}
vec3 RandomSNorm3(uint state)
{
    float a = RandomValue(state) * 2 - 1;
    float b = RandomValue(state) * 2 - 1;
    float c = RandomValue(state) * 2 - 1;
    return vec3(a, b, c);
}

vec3 HSVToRGB(vec3 hsv)
{
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
}

vec3 RGBToHSV(vec3 rgb)
{

    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = rgb.g < rgb.b ? vec4(rgb.bg, K.wz) : vec4(rgb.gb, K.xy);
    vec4 q = rgb.r < p.x ? vec4(p.xyw, rgb.r) : vec4(rgb.r, p.yzx);

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}