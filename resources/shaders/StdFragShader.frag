#version 420

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

struct Material {
    vec4 diffuseColor;
    vec4 specularColor;
    vec2 diffuseTextureUVScale;
    vec2 diffuseTextureScrollSpeed;
    vec2 diffuseTextureOffset;
    vec2 specularTextureUVScale;
    vec2 specularTextureScrollSpeed;
    vec2 specularTextureOffset;
    vec2 opacityTextureUVScale;
    vec2 opacityTextureScrollSpeed;
    vec2 opacityTextureOffset;
    float smoothnessFactor;
    float opacity;
    float alphaCutoff;
    float gamma;
    int useDiffuseTexture;
    int useSpecularTexture;
    int useOpacityTexture;
    int useLighting;
    int useGammaCorrection;
    int opacityMasked;
};
layout(std140, binding = 2) uniform TransformBlock {
    mat4 camera;
    mat4 model;
};
layout(std140, binding = 1) uniform BoneBlock {
    mat4 boneMatrices[MAX_BONES];
    int boneCount;
    int padding1;
	int padding2;
	int padding3;
};
layout(std140, binding = 3) uniform MaterialBlock {
    Material materials[8];
};

struct Light {
    vec4 position;
    vec4 intensities;
    vec4 lightDirection;
    float brightness;
    float attenuation;
    float ambiance;
    float coneAngle;
};
layout(std140, binding = 4) uniform LightBlock {
    Light lights[10];
    int lightCount;
    int lightPadding1;
    int lightPadding2;
    int lightPadding3;
};

//Input
uniform vec3 cameraPosition;
uniform float timeSec;
uniform vec3 fogColour;
uniform vec2 fogRange;   //distance where the fog starts and where it is complete; off unless the end is past the start

struct MaterialTextures {
    sampler2D diffuseTexture;
    sampler2D specularTexture;
    sampler2D opacityTexture;
};
uniform MaterialTextures materialTextures[8];

in vec2 uv;
in vec3 normal;
in vec3 vertex;
in float matId;
in vec3 vertexColour;

out vec4 FragColor;

//Voxel meshes set triplanar: the diffuse texture is projected along the three world axes and blended by how square the
//surface faces each axis. One texel is then one block on every face, so slopes and walls are not stretched the way a single
//planar projection stretches them.
uniform int triplanar;

vec4 triplanarSample(sampler2D tex, vec3 pos, vec3 n){
    vec3 weights = pow(abs(n), vec3(4.0));
    weights /= weights.x + weights.y + weights.z + 1e-6;
    vec4 alongX = texture(tex, pos.zy);
    vec4 alongY = texture(tex, pos.xz);
    vec4 alongZ = texture(tex, pos.xy);
    return alongX * weights.x + alongY * weights.y + alongZ * weights.z;
}

//The sun's shadows: shadowPass is set while the shadow map is drawn, and shadowsOn while surfaces look it up.
uniform int shadowPass;
uniform int shadowsOn;
uniform mat4 shadowMatrix;
uniform float shadowTexel;
uniform sampler2D shadowMap;

//How much of the sun reaches a surface: 1 in the light, 0 in shadow, softened over a 3 by 3 block of map texels.
float shadowAmount(vec3 pos, vec3 normal, vec3 towardsLight){
    //Push the lookup off the surface along its normal, so a surface does not shadow itself.
    vec4 lightSpace = shadowMatrix * vec4(pos + normal * 0.04, 1.0);
    vec3 uvz = lightSpace.xyz / lightSpace.w * 0.5 + 0.5;
    if(uvz.x < 0.0 || uvz.x > 1.0 || uvz.y < 0.0 || uvz.y > 1.0 || uvz.z > 1.0) return 1.0;
    float bias = max(0.0006 * (1.0 - dot(normal, towardsLight)), 0.0002);
    float lit = 0.0;
    for(int x = -1; x <= 1; ++x){
        for(int y = -1; y <= 1; ++y){
            float depth = texture(shadowMap, uvz.xy + vec2(x, y) * shadowTexel).r;
            lit += (uvz.z - bias > depth) ? 0.0 : 1.0;
        }
    }
    return lit / 9.0;
}

vec3 applyLight(Light light, vec3 surfColor, vec3 surfNormal, vec3 surfPos, vec3 surfViewDir, int mtl){
    vec3 lightIntensities = light.intensities.xyz;
    vec3 surfLightDir;
    float lightPercentage = 1.0;
    if(light.position.w == 0.0) {
        //Directional Light: the sun, which the shadow map darkens where something stands in its way
        surfLightDir = normalize(-light.lightDirection.xyz);
        lightPercentage = shadowsOn != 0 ? shadowAmount(surfPos, surfNormal, surfLightDir) : 1.0;
    } else {
        //Point Light
        surfLightDir = light.position.xyz - surfPos;
        float distanceToLight = length(surfLightDir);
        surfLightDir = normalize(surfLightDir);

        float attenuation = light.attenuation * pow(distanceToLight, 2);
        lightPercentage = 1.0 / (1.0 + attenuation);

        //cone restrictions (affects attenuation)
        float lightToSurfAngle = degrees(acos(dot(-surfLightDir, normalize(light.lightDirection.xyz))));
        if(light.coneAngle <= 90.0 && lightToSurfAngle > light.coneAngle){
            lightPercentage = 0.0;
        }
    }

    //Ambient lighting
    vec3 ambient = light.ambiance * surfColor.rgb * lightIntensities;

    //Diffuse lighting
    float diffusion = max(0.0, dot(surfNormal, surfLightDir));
    vec3 diffuse = diffusion * surfColor.rgb * lightIntensities;
    
    //Specular lighting
    float specularity = 0.0;
    vec3 specularColor = materials[mtl].specularColor.rgb;
    float specularPower = max(1.0,materials[mtl].smoothnessFactor);
    float specularOpacity = clamp(materials[mtl].smoothnessFactor,0.0,1.0);
    vec3 specularMap = vec3(1,1,1);
    if(materials[mtl].useSpecularTexture != 0){
        vec2 specularUV = (uv * materials[mtl].specularTextureUVScale);
        specularUV += (materials[mtl].specularTextureScrollSpeed*timeSec);
        specularUV += materials[mtl].specularTextureOffset;
        specularMap = texture(materialTextures[mtl].specularTexture, specularUV).rgb;
    }
    if(diffusion > 0.0){
        vec3 reflectDir = reflect(-surfLightDir, surfNormal);
        specularity = pow(max(0.0, dot(surfViewDir, reflectDir)), specularPower);
    }
    vec3 specular = specularMap * (specularity * specularOpacity * specularColor * lightIntensities);

    //Linear lighting color
    return max(light.brightness,0.0) * (ambient + lightPercentage*(diffuse + specular));
}

void main() {
    //The shadow map pass only records depth: no colour is needed there
    if(shadowPass != 0){
        FragColor = vec4(0.0);
        return;
    }

    //World-space surface position and normal: the vertex shader already applied the model transform to both
    vec3 surfPos = vertex;
    vec4 surfColor = vec4(1.0);
    float surfOpacity = 1;
    int mtl = int(matId);

    //Material Diffuse - Calculated without lighting
    vec4 diffuseColor = vec4(materials[mtl].diffuseColor.rgb,1);                               //0-1, as given by the material
    if(materials[mtl].useDiffuseTexture != 0){                                                       //Surface color is derived from diffuse texture color * material diffuse color, if set
        vec2 diffuseUV = (uv * materials[mtl].diffuseTextureUVScale);
        diffuseUV += (materials[mtl].diffuseTextureScrollSpeed*timeSec);
        diffuseUV += materials[mtl].diffuseTextureOffset;
        if(triplanar != 0){
            surfColor = triplanarSample(materialTextures[mtl].diffuseTexture, surfPos, normalize(normal));
        } else {
            surfColor = texture(materialTextures[mtl].diffuseTexture, diffuseUV);
        }
    }
    surfColor *=  diffuseColor;                                                                 //Modulate diffuse texture color with diffuse color
    surfColor.rgb *= vertexColour;                                                              //Per-vertex tint (white unless the mesh sets it)

    //Material Opacity --   Calculated without lighting
    float opacity = clamp(materials[mtl].opacity,0.0,1.0);                                      //Normalize material opacity
    if(materials[mtl].useOpacityTexture != 0){                                                       //Surface opacity is derived from min of opacity texture color and material opacity, if set
        vec2 opacityUV = (uv * materials[mtl].opacityTextureUVScale);
        opacityUV += materials[mtl].opacityTextureScrollSpeed*timeSec;
        opacityUV += materials[mtl].opacityTextureOffset;
        vec4 opacityColor = texture(materialTextures[mtl].opacityTexture, opacityUV);
        float opacityIntensity = opacityColor.a;
        if(materials[mtl].opacityMasked == 0){
            surfOpacity = opacityIntensity;
        } else {
            if(opacityIntensity < materials[mtl].alphaCutoff){
                surfOpacity = 0;
            }
        }
    }
    if(surfOpacity < materials[mtl].alphaCutoff) discard;                                       //Discard fragments with alpha below the cutoff

    //Surface lighting
    //Unlit materials show their surface colour. Lit ones get only what the lights give (ambient fill + direct light);
    //adding the full unlit colour as well made every lit surface at least as bright as its texture and overexposed white ones.
    vec3 linearColor = surfColor.rgb;
    if(materials[mtl].useLighting != 0){
        linearColor = vec3(0.0);
        vec3 surfNormal = normalize(normal);
        vec3 surfViewDir = normalize(cameraPosition - surfPos);

        for(int i = 0; i < lightCount; ++i){
            if(lights[i].brightness > 0){
                linearColor += applyLight(lights[i], surfColor.rgb, surfNormal, surfPos, surfViewDir, mtl);
            }
        }
    }

	//Post-lighting Gamma correction
    vec3 fragColor = linearColor.rgb;
    if(materials[mtl].useGammaCorrection != 0){
        if(materials[mtl].gamma < 0.1) {
            fragColor = linearColor;
        } else {
            vec3 gammaCorrection = vec3(1.0/materials[mtl].gamma);
            fragColor = pow(linearColor, gammaCorrection);
        }
    }
    
    //Distance fog: fades the surface into the fog colour between the start and end distances
    if(fogRange.y > fogRange.x){
        float fogAmount = clamp((length(surfPos - cameraPosition) - fogRange.x) / (fogRange.y - fogRange.x), 0.0, 1.0);
        fragColor = mix(fragColor, fogColour, fogAmount);
    }

    FragColor = vec4(fragColor, surfOpacity);
}