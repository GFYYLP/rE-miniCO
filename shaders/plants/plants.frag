#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) flat in int isLeafCurr;
layout(location = 4) flat in uint dnaID;
layout(location = 5) in float effectiveAge;
layout(location = 6) in float energy;

layout(location = 0) out vec4 outFrag;

const float BARK_NEUTRAL_HUE = 30.0/360.0; 
const float LEAF_GREEN_HUE = 110.0/360.0;
const float LEAF_BROWN_HUE = 360.0/360.0;

#extension GL_GOOGLE_include_directive : enable
#include "utilities/globals.glsl"
#include "utilities/plants.glsl"
#include "utilities/lighting.glsl"


void main() {
    vec3 lighting = vec3(0.0);
    float alpha = 1.0;
    float dnaHue = pDNA.hue[dnaID];
    
    //smooth age for gentler transitions
    float ageSmooth = smoothstep(0.0, 1.0, effectiveAge);
    float energySmooth = smoothstep(0.0, 1.0, energy);
    float maturity = smoothstep(0.2, 0.8, ageSmooth*0.1);
    float senescence = smoothstep(0.4, 1.0, effectiveAge*0.1);

    if (isLeafCurr == 1 || isLeafCurr == 3) {
        //leaves  
        float zoom = mix(0.15, 1.0, ageSmooth*0.2);// * (1.0 - senescence * 0.75);;
        vec2 maskUV = vec2(0.5) + (vec2(inUV.x, inUV.y * 0.5) - vec2(0.5)) * zoom;
        vec4 p = texture(plantmasks, vec3(maskUV, dnaID));

        alpha = p.x;
        if (alpha < 0.7) discard;
        
        //balance between spherical silhouette and per-leaf normal
        vec3 leafNormal = normalize(p.yzw);
        vec3 sphericalNormal = normalize(inNormal);
        float sphericalWeight = 0.5;
        vec4 N = vec4(normalize(mix(leafNormal, sphericalNormal, sphericalWeight)), 0.0);

        //build age/energy-sensitive ambient color
        float hue = mix(LEAF_GREEN_HUE, dnaHue, 0.02 + 0.03 * maturity);
        float sat = mix(0.5, 0.8, energySmooth);
        float val = mix(0.04, 0.08, energySmooth) * mix(1.0, 0.85, ageSmooth); // Very low brightness
        // float hue = mix(LEAF_GREEN_HUE, LEAF_BROWN_HUE, senescence * 0.8 + 0.02 + 0.03 * maturity * dnaHue);
        // float sat = mix(0.5, 0.8, energySmooth) * (1.0 - senescence * 0.6);
        // float val = mix(0.04, 0.08, energySmooth) * mix(1.0, 0.85, ageSmooth);
        
        vec3 baseColor = hsv2rgb(vec3(hue, sat, val));
        lighting = calculateLighting(baseColor, N.xyz, 0.0);

    } else if (isLeafCurr == 2) {
        //flowers 
        vec4 p = texture(plantmasks, vec3(inUV.x, inUV.y * 0.5 + 0.5, dnaID));
        alpha = p.x;
        if (alpha < 0.5) discard;

        vec3 flowerNormal = normalize(p.yzw);

        //build age-sensitive ambient color, retain more hue than leaves
        float hue = dnaHue; 
        float sat = mix(0.6, 0.85, energySmooth);
        float val = mix(0.08, 0.15, energySmooth);
        vec3 baseColor = hsv2rgb(vec3(hue, sat, val));

        lighting = calculateLighting(baseColor, flowerNormal, 10.0);

    } else {
        //barks   
        //retain hue subtly
        float hue = mix(BARK_NEUTRAL_HUE, dnaHue, 0.03);
        float sat = 0.2;
        float val = mix(0.025, 0.018, ageSmooth); 
        vec3 baseColor = hsv2rgb(vec3(hue, sat, val));

        lighting = calculateLighting(baseColor, inNormal, 0.0);
    }


    outFrag = vec4(lighting, alpha); 
}