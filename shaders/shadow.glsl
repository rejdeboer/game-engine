
float calculateShadowFactor(vec4 posLightSpace, vec3 normal, vec4 sunlightDirection, sampler2DShadow shadowMapTex) {
    // 1. Perspective Divide: Convert from clip space to NDC [-1, 1]
    vec3 projCoords = posLightSpace.xyz / posLightSpace.w;

    // 2. Transform to Texture Coordinates [0, 1]
    vec2 shadowTexCoords = projCoords.xy * 0.5 + 0.5;

    float bias = max(0.005 * tan(acos(dot(normalize(normal), normalize(sunlightDirection.xyz)))), 0.001);
    float shadow = texture(shadowMapTex, vec3(shadowTexCoords, projCoords.z - bias));

    // Optional: Clamp coords or check depth bounds if needed
    // (Sampler border mode CLAMP_TO_BORDER with white often handles points outside the frustum)
    if (projCoords.z > 1.0) { // If beyond light's far plane
        shadow = 1.0; // Consider it lit
    }

    // TODO: Consider optionals
    // Optional: Basic PCF (Percentage-Closer Filtering) for softer shadows
    // float pcfShadow = 0.0;
    // vec2 texelSize = 1.0 / textureSize(shadowMapSampler, 0); // Get size of one texel
    // int pcfRadius = 1; // Sample a 3x3 area (radius 1)
    // for(int x = -pcfRadius; x <= pcfRadius; ++x) {
    //     for(int y = -pcfRadius; y <= pcfRadius; ++y) {
    //         pcfShadow += texture(shadowMapSampler, vec3(projCoords.xy + vec2(x, y) * texelSize, projCoords.z));
    //     }
    // }
    // shadow = pcfShadow / ((pcfRadius*2+1)*(pcfRadius*2+1)); // Average the results

    return shadow;
}
