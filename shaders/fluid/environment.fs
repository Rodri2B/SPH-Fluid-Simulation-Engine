#version 460 core
out vec4 FragColor;

Texture3D<float4> DensityMap;

float indexOfRefraction = 1.33;
int numRefractions = 4;
vec3 extinctionCoeff = vec3(12.0,4.0,4.0);

float volumeValueOffset = 200.0;

float lightStepSize = 0.1;

// StepSize = 0.02 raymarching only

float densityMultiplier = 0.05/ 1000.0;//0.05

float TinyNudge = 0.01;

// Test-environment settings
vec3 dirToSun = vec3(0.4,0.4,0.4);

vec4 tileCol1 = vec4(0.204,0.318,0.776,1.0);
vec4 tileCol2 = vec4(0.608,0.369,0.858,1.0);
vec4 tileCol3 = vec4(0.302,0.736,0.458,1.0);
vec4 tileCol4 = vec4(0.802,0.643,0.367,1.0);
vec3 tileColVariation = vec3(0.33,0,0.47);
float tileScale = 1;
float tileDarkOffset = -0.35;
//const vec4x4 cubeLocalToWorld;
//const vec4x4 cubeWorldToLocal;
vec3 floorPos;
vec3 floorSize;

vec3 cubeCol = vec3(0.95, 0.3, 0.35);
float iorAir = 1.0;

vec3 boundsSize; //simulation domain

void main()
{

}


vec3 SampleSky(vec3 dir)
{
    const vec3 colGround = vec3(0.35, 0.3, 0.35) * 0.53;
    const vec3 colSkyHorizon = vec3(1, 1, 1);
    const vec3 colSkyZenith = vec3(0.08, 0.37, 0.73);

    float sun = pow(max(0, dot(dir, dirToSun)), 500) * 1;
    float skyGradientT = pow(smoothstep(0, 0.4, dir.y), 0.35);
    float groundToSkyT = smoothstep(-0.01, 0, dir.y);
    vec3 skyGradient = mix(colSkyHorizon, colSkyZenith, skyGradientT);

    return mix(colGround, skyGradient, groundToSkyT) + sun * float(groundToSkyT >= 1);
}

// Returns (dstToBox, dstInsideBox). If ray misses box, dstInsideBox will be zero
vec2 RayBoxDst(vec3 boundsMin, vec3 boundsMax, vec3 rayOrigin, vec3 rayDir)
{
    vec3 invRayDir = 1 / rayDir;
    // Adapted from: http://jcgt.org/published/0007/03/04/
    vec3 t0 = (boundsMin - rayOrigin) * invRayDir;
    vec3 t1 = (boundsMax - rayOrigin) * invRayDir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float dstA = max(max(tmin.x, tmin.y), tmin.z);
    float dstB = min(tmax.x, min(tmax.y, tmax.z));
    // CASE 1: ray intersects box from outside (0 <= dstA <= dstB)
    // dstA is dst to nearest intersection, dstB dst to far intersection
    // CASE 2: ray intersects box from inside (dstA < 0 < dstB)
    // dstA is the dst to intersection behind the ray, dstB is dst to forward intersection
    // CASE 3: ray misses box (dstA > dstB)
    float dstToBox = max(0, dstA);
    float dstInsideBox = max(0, dstB - dstToBox);
    return vec2(dstToBox, dstInsideBox);
}



float SampleDensity(vec3 pos)
{
    vec3 uvw = (pos + boundsSize * 0.5) / boundsSize;

    const float epsilon = 0.0001;
    bool isEdge = 
    (any(lessThanEqual(uvw, vec3(epsilon))) || any(greaterThanEqual(uvw, vec3(1.0 - epsilon))));

    if (isEdge) return -volumeValueOffset;

    return DensityMap.SampleLevel(linearClampSampler, uvw, 0).r - volumeValueOffset;
}

float CalculateDensityAlongRay(vec3 rayPos, vec3 rayDir, float stepSize)
{
    // Test for non-normalize ray and return 0 in that case.
    // This happens when refract direction is calculated, but ray is totally reflected
    if (dot(rayDir, rayDir) < 0.9) return 0;
    vec2 boundsDstInfo = RayBoxDst(-boundsSize * 0.5, boundsSize * 0.5, rayPos, rayDir);
    float dstToBounds = boundsDstInfo[0];
    float dstThroughBounds = boundsDstInfo[1];
    if (dstThroughBounds <= 0) return 0;
    float dstTravelled = 0;
    float opticalDepth = 0;
    float nudge = stepSize * 0.5;
    vec3 entryPoint = rayPos + rayDir * (dstToBounds + nudge);
    dstThroughBounds -= (nudge + TinyNudge);
    while (dstTravelled < dstThroughBounds)
    {
        rayPos = entryPoint + rayDir * dstTravelled;
        float density = SampleDensity(rayPos) * densityMultiplier * stepSize;
        if (density > 0)
        {
            opticalDepth += density;
        }
        dstTravelled += stepSize;
    }
    return opticalDepth;
}


vec3 Transmittance(float thickness)
{
    return exp(-thickness * extinctionCoeff);
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

vec3 HSVToRGB(vec3 hsv)
{
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
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
float RandomSNorm(uint state)
{
    return RandomValue(state) * 2 - 1;
}
vec3 RandomSNorm3(uint state)
{
    float a = RandomValue(state) * 2 - 1;
    float b = RandomValue(state) * 2 - 1;
    float c = RandomValue(state) * 2 - 1;
    return vec3(a, b, c);
}

vec3 SampleEnvironment(vec3 pos, vec3 dir)
{
    HitInfo floorInfo = RayBox(pos, dir, floorPos, floorSize);
    HitInfo cubeInfo = RayBoxWithMatrix(pos, dir, cubeLocalToWorld, cubeWorldToLocal);

    if (cubeInfo.didHit && cubeInfo.dst < floorInfo.dst)
    {
        return clamp(dot(cubeInfo.normal, dirToSun) * 0.5 + 0.5, 0.0, 1.0) * cubeCol;
    }
    else if (floorInfo.didHit)
    {
        // Choose tileCol based on quadrant
        vec3 tileCol = floorInfo.hitPoint.x < 0 ? tileCol1 : tileCol2;
        if (floorInfo.hitPoint.z < 0) tileCol = floorInfo.hitPoint.x < 0 ? tileCol3 : tileCol4;

        // If tile is a dark tile, then darken it
        ivec2 tileCoord = floor(floorInfo.hitPoint.xz * tileScale);
        bool isDarkTile = Modulo(tileCoord.x, 2) == Modulo(tileCoord.y, 2);
        tileCol = TweakHSV(tileCol, vec3(0, 0, tileDarkOffset * isDarkTile));

        // Vary hue/sat/val randomly
        uint rngState = HashInt2(tileCoord);
        vec3 randomVariation = RandomSNorm3(rngState) * tileColVariation * 0.1;
        tileCol = TweakHSV(tileCol, randomVariation);

        vec3 shadowMap = Transmittance(CalculateDensityAlongRay(floorInfo.hitPoint, _WorldSpaceLightPos0, lightStepSize * 2) * 2);
        bool inShadow = RayBoxWithMatrix(floorInfo.hitPoint, dirToSun, cubeLocalToWorld, cubeWorldToLocal).didHit;
        if (inShadow) shadowMap *= 0.2;
        return tileCol * shadowMap;
    }

    return SampleSky(dir);
}