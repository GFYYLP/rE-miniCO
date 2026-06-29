#version 450

layout (location = 0) in vec2 fragUV;
layout (location = 0) out vec4 outColor;

#extension GL_GOOGLE_include_directive : enable
#include "utilities/globals.glsl"

//13-tap Gaussian kernel
const float KERNEL[7] = float[](
    0.234598,
    0.195621,
    0.124216,
    0.069166,
    0.028837,
    0.009385,
    0.002329
);


void main() {
    vec4 result = vec4(0.0);
    
    //vertical blur
    for (int i = -6; i <= 6; ++i) {
        vec2 offset = vec2(0.0, float(i) * globals.texelSizeY * globals.blurRadius);
        vec2 sampleUV = fragUV + offset;
        
        vec4 color = texture(heightmapSampler[nonuniformEXT(pc.samplerIndex)], sampleUV);
        
        // Boost brighter areas
        float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
        float boost = 1.0 + brightness * globals.brightnessBoost;
        
        int kernelIdx = abs(i);
        result += color * KERNEL[kernelIdx] * boost;
    }
    
    outColor = result;
}