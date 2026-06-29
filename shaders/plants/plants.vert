#version 450
#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in mat4 instanceModel;

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) flat out int isLeafCurr;
layout(location = 4) flat out uint dnaID;
layout(location = 5) out float effectiveAge;
layout(location = 6) out float energy;

#extension GL_GOOGLE_include_directive : enable
#include "utilities/globals.glsl"
#include "utilities/plants.glsl"


void main() {
    uint entityID = gl_DrawIDARB;
    uint transformID = gl_InstanceIndex;
    dnaID = pInstances.dna[entityID];
    outNormal = inNormal;

    mat3x4 r = pTransforms.transform[entityID][transformID];
    vec4 t = pTransforms.translation[entityID][transformID];
    vec3 worldT = pTransforms.worldOffset[entityID].xyz;
    
    vec3 right = r[0].xyz;
    vec3 up = r[1].xyz;
    vec3 forward = r[2].xyz;
    vec3 localT = t.xyz;  
    float scale = t.w;
    isLeafCurr = int(r[2].w);
    energy = pInstances.energy[entityID];

    float age = pInstances.age[entityID] * 10.0;
    vec3 ageFactor = vec3(0.0, 0.5, 0.0) * age;
    float senescence = 0.0;//smoothstep(0.7, 1.0, pInstances.age[entityID]);

    int generation = (isLeafCurr > 0)? 2 : int(r[0].w);
    float generationDelay = float(generation) * 0.15;

    effectiveAge = max(0.0, age - generationDelay);
    float growthFactor = 1.0 - exp(-effectiveAge * 2.0);

    float radius = scale * smoothstep(0.2, 0.6, effectiveAge) * (1.0 - senescence);
    float branchLength = (scale * 10.0) * smoothstep(0.0, 0.3, effectiveAge) * (1.0 - senescence);


    //billboard handling for trunk and foliage
    if (generation == 0 || isLeafCurr == 1) {
        right = vec3(-sin(globals.cameraAz), 0.0, cos(globals.cameraAz));
        if (isLeafCurr > 0) {
            branchLength /= 6.0;
            ageFactor = vec3(0.0, 0.2, 0.0) * age * 0.1;
        } else {
            ageFactor = vec3(0.0, 0.2, 0.0) * age * 0.1;
        }
    }
    
    //apply transform
    vec3 localPos = right * ((inPos.x - 0.5) * radius)
                + forward * (inPos.z * branchLength);
    localPos *= (isLeafCurr >= 1 ? 2.0 : 1.0);
    localT.y = mix(0.0, t.y, growthFactor);  //more noticeable vertical growth
    vec3 worldPos = localT + localPos + worldT;

    //wind sway
    if (isLeafCurr == 1) {
        float time = globals.delta * globals.timeSpeed * 0.05;
        float swayAmount = inPos.z * 2.0;
        float sway = sin(time + float(transformID) * 0.8) * swayAmount;
        worldPos.xy += vec2(sway);
    }
    else if (isLeafCurr == 3) {  //soft plants sway in both x and z directions for a more dynamic effect
        float time = globals.delta * globals.timeSpeed * 0.03;
        float sway = sin(time + float(transformID) * 1.2) * inPos.z * 5.8;
        worldPos.xz += vec2(sway * forward.x, sway * forward.z);
    }


    //spherical normal approx
    vec3 center = vec3(r[0].w, worldT.y + localT.y, r[1].w);  
    if (isLeafCurr == 1) outNormal = normalize(worldPos - center);
    
    outPos = worldPos;
    outUV = inUV;
    gl_Position = pc.VP * vec4(worldPos, 1.0);
}