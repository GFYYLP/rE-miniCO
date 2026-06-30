#ifndef HELPER_GLSL
#define HELPER_GLSL


const float shadowHueShift = -0.12;      // Cool shift for shadows
const float shadowDesaturation = 0.75;   // Slightly desaturate shadows
const float shadowBrightness = 0.01;     // Shadow darkness
const float wrapAmount = 0.6;            // Soften shadow terminator 
//const vec3 rimColor = vec3(0.6, 0.7, 1.0); // Sky-like rim
const vec3 groundBounce = vec3(0.8, 0.6, 0.4); // Warm ground reflection



vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 generateShadowColor(vec3 baseColor) {
    vec3 hsv = rgb2hsv(baseColor);
    hsv.x = fract(hsv.x + shadowHueShift);  // Hue shift towards cool
    hsv.y *= shadowDesaturation;             // Desaturate slightly
    hsv.z *= shadowBrightness;               // Darken
    return hsv2rgb(hsv);
}

vec3 calculateLighting(vec3 ambient, vec3 normalParam, float shininess){
    vec3 viewDir = normalize(globals.cameraPos - inPos);
    vec3 L = normalize(-globals.lightDir);
    vec3 N = normalize(normalParam);
    float NdotL = dot(N, L);
    
    //diffuse
    vec3 diffuse = mix(generateShadowColor(ambient.rgb),   //shadow color
                       ambient.rgb * globals.lightColor.rgb,  //lit color
                       clamp((NdotL + wrapAmount) / (1.0 + wrapAmount), 0.0, 1.0));  //wrapped diffuse to soften shadows and fake some SSS
    
    //specular (warm accent)
    //float spec = 0.0;
    //if (shininess > 0.0 && NdotL > 0.0) spec = pow(max(dot(viewDir, reflect(-L, N)), 0.0), shininess); 

    vec3 H = normalize(L + viewDir);
    float spec = (shininess > 0.0 && NdotL > 0.0)
    ? pow(max(dot(N, H), 0.0), shininess * 4.0)
    : 0.0;
    vec3 specular = spec * globals.lightColor.rgb * vec3(1.0, 0.95, 0.9);


    //shadow
    vec4 fragPosLightSpace = globals.lightVP * vec4(inPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float shadow = 0.0;
    //float slopeBias = tan(acos(clamp(NdotL, 0.0, 1.0)));
    float slopeBias = sqrt(max(0.0, 1.0 - NdotL * NdotL)) / max(NdotL, 0.001);
    float bias = clamp(0.002 + 0.005 * slopeBias, 0.002, 0.025);

    ivec2 hmSize = textureSize(heightmapSampler[nonuniformEXT(pc.samplerIndex)], 0);
    vec2 texelSize = 1.0 / vec2(hmSize);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(heightmapSampler[nonuniformEXT(1)], projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    if (projCoords.z > 1.0) shadow = 0.0;
    //shadow *= globals.dayProgress;

    //ground bounce — warm fill on shadow-facing surfaces, tinted by current light
    float shadowFacing = clamp(-NdotL * 0.5 + 0.5, 0.0, 1.0);
    //vec3 bounce = globals.lightColor.rgb * groundBounce * shadowFacing * 0.15;

    float bounceStrength = shadowFacing * (shadow * 0.25 + (1.0 - shadow) * 0.15);
    vec3 bounce = globals.lightColor.rgb * groundBounce * bounceStrength;

    //return ambient.rgb + (diffuse + specular * 2.0) * (1.0 - shadow) + bounce;

    float ambientOcclusion = 1.0 - shadow * 0.4; // shadow softly bleeds into ambient
    return ambient.rgb * ambientOcclusion + (diffuse * 3.0 + specular * 2.0) * (1.0 - shadow) + bounce;
}


#endif