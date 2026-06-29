#version 450

layout (location = 0) in vec2 fragUV;
layout (location = 0) out vec4 outColor;

#extension GL_GOOGLE_include_directive : enable
#include "utilities/globals.glsl"


void main() {
    vec3 scene = texture(heightmapSampler[4], fragUV).rgb;  //base framebuffer
    vec3 bloom  = texture(heightmapSampler[nonuniformEXT(pc.samplerIndex)], fragUV).rgb * 2.0;
    float centerDepthVal = texture(heightmapSampler[9], fragUV).r;  //depth buffer value
    vec3 skyAmbient = globals.secondlightColor.rgb * globals.dayProgress * 0.5;

//god rays
    #include "postprocess/godray.glsl"

//edge detection + directional outline
    #include "postprocess/outline.glsl"

//composite 
    vec3  coloredGradient = mix(globals.secondlightColor.rgb, base, fragUV.y);  //color washing on screen upper half for depth
    vec3  gradient = mix(base, coloredGradient, 1.0);

    vec3 final = base + skyAmbient + bloom + godRayContrib + gradient * 0.7;

    outColor  = vec4(final, 1.0);


//display framebuffer (for debugging)
    if (pc.renderTop == 1){
        vec3 depthMap = texture(heightmapSampler[8], fragUV).rgb;
        outColor = vec4(depthMap, 1.0) * 2.0;

        vec3 aMap = texture(heightmapSampler[7], fragUV).rgb;
        outColor += vec4(aMap, 0.0) * 0.1;
    }

    // vec3 aMapa = texture(heightmapSampler[1], fragUV).rgb;
    // outColor = vec4(aMapa, 1.0) * 2.0;
}
