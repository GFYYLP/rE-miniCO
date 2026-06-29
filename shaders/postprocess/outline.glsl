#ifndef OUTLINE_GLSL
#define OUTLINE_GLSL

    float depthThreshold = 0.002 * centerDepthVal;

    vec2  texel          = vec2(globals.texelSizeX, globals.texelSizeY);
    vec2 offsets[4] = vec2[](
        vec2( texel.x,  0.0),
        vec2(-texel.x,  0.0),
        vec2( 0.0,  texel.y),
        vec2( 0.0, -texel.y)
    );


    //edge detection
    float depths[4];
    vec3 colorAccum = vec3(0.0);
    float depthEdge      = 0.0;

    for (int i = 0; i < 4; i++) {
        depths[i] = texture(heightmapSampler[9], fragUV + offsets[i]).r;

        //edge accumulation scales with difference's magnitude with the neighboring texels
        depthEdge += smoothstep(depthThreshold, depthThreshold * 1.5, abs(centerDepthVal - depths[i]));

        colorAccum += texture(heightmapSampler[4], fragUV + offsets[i]).rgb;
    }
    depthEdge  /= 4.0;
    colorAccum /= 4.0;
    float edge = smoothstep(0.1, 0.45, depthEdge);


    //luminance filtering (directional light masking)
    vec2 depthGrad  = vec2(depths[0] - depths[1], depths[2] - depths[3]);  //depth gradient approx
    float lightAlign = dot(normalize(depthGrad + vec2(1e-5)), globals.screenLightDir); //projects onto screen light dir
    float maxLum = max(dot(scene, vec3(0.299, 0.587, 0.114)), 
                    dot(colorAccum, vec3(0.299, 0.587, 0.114)));
    float litMask = clamp(lightAlign, 0.0, 1.0) * smoothstep(0.05, 0.35, maxLum);  //light dir alignment + luminence value
    edge *= litMask;


    vec3 litOutlineColor = globals.secondlightColor.rgb * 1.2;
    vec3 base = mix(scene, litOutlineColor, edge * 2.8);

#endif