#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) flat in int isLeafCurr;
layout(location = 4) flat in uint dnaID;
layout(location = 5) in float effectiveAge;
layout(location = 6) in float energy;

layout(location = 0) out vec4 outFrag;

#extension GL_GOOGLE_include_directive : enable
#include "utilities/globals.glsl"
#include "utilities/plants.glsl"



void main() {
//plant masks
    float finalAlpha = 1.0;
    float ageSmooth = smoothstep(0.0, 1.0, effectiveAge);

    if (isLeafCurr == 1 || isLeafCurr == 3) {
        //uint idx = uint(inUV.y * BITMASK_COUNT) * BITMASK_COUNT + uint(inUV.x * BITMASK_COUNT);
        //MaskPixels currPix = pDNA.dna[0].leafMasks[idx];
        //finalAlpha = texture(plantmasks, vec3(vec2(inUV.x, inUV.y * 0.5), dnaID)).x;

        float zoom = mix(0.15, 1.0, ageSmooth*0.2);
        vec2 maskUV = vec2(0.5) + (vec2(inUV.x, inUV.y * 0.5) - vec2(0.5)) * zoom;
        finalAlpha = texture(plantmasks, vec3(maskUV, dnaID)).x;

        //finalAlpha = currPix.alpha / 255.0;

        if (finalAlpha < 0.7) {
            discard;
        }
    }


//final color
    outFrag = vec4(vec3(1.0), finalAlpha);

    //outFrag = vec4(inNormal, 0.5);
}
