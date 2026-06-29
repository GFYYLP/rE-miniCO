#ifndef GODRAY_GLSL
#define GODRAY_GLSL

    vec3 godRayContrib = vec3(0.0);
    float lightIntensity = 1.0 - max(0.0, dot(normalize(globals.lightDir), vec3(0.0, 1.0, 0.0))); //falloff when sun is below horizon

    vec4  lightDirClip   = pc.VP * vec4(globals.lightDir, 0.0);
    vec2  lightDirScreen = normalize(lightDirClip.xy);

    //march against light direction from current pixel upward toward the occluder
    const int   RAY_SAMPLES = 40;
    const float RAY_SPACING = 0.092;
    const float RAY_DECAY   = 0.95;
    const float RAY_WEIGHT  = 0.78;

    vec2 toEdge = mix(vec2(0.0), vec2(1.0),                 //more samples when close to edge, fewer when safely in middle
                    step(vec2(0.0), -lightDirScreen)) - fragUV;  // distance to edge/screen boundary in the direction of the light                  
    float edgeDist = min(abs(toEdge.x / (-lightDirScreen.x + 1e-5)),  
                         abs(toEdge.y / (-lightDirScreen.y + 1e-5)));  //add small epsilon to avoid division by zero
    float dynamicSpacing = edgeDist / float(RAY_SAMPLES); // adapt step size based on distance to edge for better quality and performance
    vec2 rayStep = -lightDirScreen * dynamicSpacing;

    float accumulation = 0.0;
    float decayFactor  = 0.1;
    float jitter = fract(sin(dot(fragUV + vec2(globals.delta * 0.13), vec2(127.1, 311.7))) * 43758.5);  //noise-offset march uv to hide banding
    vec2 sampleUV = fragUV + rayStep * jitter;


    //aacumulate occlusion
    float blocked = 0.0;
    for (int i = 0; i < RAY_SAMPLES; i++) {
        sampleUV += rayStep;

        float marchDepthVal = texture(heightmapSampler[9], sampleUV).r;
        float depthDelta  = centerDepthVal - marchDepthVal;

        //hard cutoff instead of smoothstep for crisp pixelated edges
        float hitOccluder = 1.0 - step(depthDelta, 0.001);
        blocked = max(blocked, hitOccluder);  

        if (blocked >= 1.0) break;

        accumulation += (1.0 - blocked) * decayFactor * RAY_WEIGHT;
        decayFactor  *= RAY_DECAY;
    }

    godRayContrib = accumulation * globals.lightColor.rgb * lightIntensity * 0.3;

#endif