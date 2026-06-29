#version 450

layout (location = 0) in vec2 fragUV;
layout (location = 0) out vec4 outColor;

#extension GL_GOOGLE_include_directive : enable
#include "utilities/globals.glsl"

const float THRESHOLD = 0.5;
const float INTENSITY = 5.0;  
const float SOFT_RANGE = 6.0;


void main() {
    vec4 color = texture(heightmapSampler[nonuniformEXT(pc.samplerIndex)], fragUV);
    float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    
    //extract brightness
    float mask = 0.0;
    if (brightness > THRESHOLD + SOFT_RANGE) {
        mask = 1.0;
    } else if (brightness > THRESHOLD) {
        mask = (brightness - THRESHOLD) / SOFT_RANGE; //smooth transition for texels within soft range
    }
    
    outColor = color * mask * INTENSITY;
}