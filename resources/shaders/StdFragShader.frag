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

out vec4 FragColor;

vec3 applyLight(Light light, vec3 surfColor, vec3 surfNormal, vec3 surfPos, vec3 surfViewDir, int mtl){
    vec3 lightIntensities = light.intensities.xyz;
    vec3 surfLightDir;
    float lightPercentage = 1.0;
    if(light.position.w == 0.0) {
        //Directional Light
        surfLightDir = normalize(-light.lightDirection.xyz);
        lightPercentage = 1.0;
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
    vec3 specularColor = normalize(materials[mtl].specularColor.rgb);
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
    //Model-space surface position
    vec3 surfPos = vec3(model * vec4(vertex, 1));
    vec4 surfColor = vec4(1.0);
    float surfOpacity = 1;
    int mtl = int(matId);

    //Material Diffuse - Calculated without lighting
    vec4 diffuseColor = vec4(normalize(materials[mtl].diffuseColor.rgb),1);
    if(materials[mtl].useDiffuseTexture != 0){                                                       //Surface color is derived from diffuse texture color * material diffuse color, if set
        vec2 diffuseUV = (uv * materials[mtl].diffuseTextureUVScale);
        diffuseUV += (materials[mtl].diffuseTextureScrollSpeed*timeSec);
        diffuseUV += materials[mtl].diffuseTextureOffset;
        surfColor = texture(materialTextures[mtl].diffuseTexture, diffuseUV);
    }
    surfColor *=  diffuseColor;                                                                 //Modulate diffuse texture color with diffuse color

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
    vec3 linearColor = surfColor.rgb;
    if(materials[mtl].useLighting != 0){
        vec3 surfNormal = normalize(transpose(inverse(mat3(model))) * normal);
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
    
    FragColor = vec4(fragColor, surfOpacity);
}