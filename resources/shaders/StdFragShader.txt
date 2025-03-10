#version 330

#define MAX_LIGHTS 10
#define MAX_MATERIALS 8

//Input
uniform mat4 model;
uniform vec3 cameraPosition;

uniform struct Light {
   vec4 position;
   float brightness;
   vec3 intensities;
   float attenuation;
   float ambiance;
   float coneAngle;
   vec3 lightDirection;
} lights[MAX_LIGHTS];
uniform int lightCount;

uniform struct Material {
    vec3 diffuseColor;
    bool useDiffuseTexture;
    sampler2D diffuseTexture;
    vec2 diffuseTextureUVScale;
    vec2 diffuseTextureScrollSpeed;
    vec2 diffuseTextureOffset;
    bool useSpecularTexture;
    sampler2D specularTexture;
    vec2 specularTextureUVScale;
    vec2 specularTextureScrollSpeed;
    vec2 specularTextureOffset;
    vec3 specularColor;
    float smoothnessFactor;
    bool useOpacityTexture;
    sampler2D opacityTexture;
    vec2 opacityTextureUVScale;
    vec2 opacityTextureScrollSpeed;
    vec2 opacityTextureOffset;
    float opacity;
    float alphaCutoff;
    bool opacityMasked;
    float gamma;
    bool useGammaCorrection;
    bool useLighting;
} materials[MAX_MATERIALS];
uniform float timeSec;

varying vec2 uv;
varying vec3 normal;
varying vec3 vertex;
varying float matId;

vec3 applyLight(Light light, vec3 surfColor, vec3 surfNormal, vec3 surfPos, vec3 surfViewDir, int mtl){
    vec3 lightIntensities = normalize(light.intensities);
    vec3 surfLightDir;
    float lightPercentage = 1.0;
    if(light.position.w == 0.0) {
        //Directional Light
        surfLightDir = normalize(-light.lightDirection);
        lightPercentage = 1.0;
    } else {
        //Point Light
        surfLightDir = light.position.xyz - surfPos;
        float distanceToLight = length(surfLightDir);
        surfLightDir = normalize(surfLightDir);

        float attenuation = light.attenuation * pow(distanceToLight, 2);
        lightPercentage = 1.0 / (1.0 + attenuation);

        //cone restrictions (affects attenuation)
        float lightToSurfAngle = degrees(acos(dot(-surfLightDir, normalize(light.lightDirection))));
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
    vec3 specularColor = normalize(materials[mtl].specularColor);
    float specularPower = max(1.0,materials[mtl].smoothnessFactor);
    float specularOpacity = clamp(materials[mtl].smoothnessFactor,0.0,1.0);
    vec3 specularMap = vec3(1,1,1);
    if(materials[mtl].useSpecularTexture){
        vec2 specularUV = (uv * materials[mtl].specularTextureUVScale);
        specularUV += (materials[mtl].specularTextureScrollSpeed*timeSec);
        specularUV += materials[mtl].specularTextureOffset;
        specularMap = texture(materials[mtl].specularTexture, specularUV).rgb;
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
    vec4 surfColor = vec4(1,1,1,1);
    float surfOpacity = 1;
    int mtl = int(matId);

    //Material Diffuse - Calculated without lighting
    vec4 diffuseColor = vec4(normalize(materials[mtl].diffuseColor),1);                         //Normalize material diffuse color
    if(materials[mtl].useDiffuseTexture){                                                       //Surface color is derived from diffuse texture color * material diffuse color, if set
        vec2 diffuseUV = (uv * materials[mtl].diffuseTextureUVScale);
        diffuseUV += (materials[mtl].diffuseTextureScrollSpeed*timeSec);
        diffuseUV += materials[mtl].diffuseTextureOffset;
        surfColor = texture(materials[mtl].diffuseTexture, diffuseUV);
    }
    surfColor *=  diffuseColor;                                                                 //Modulate diffuse texture color with diffuse color

    //Material Opacity --   Calculated without lighting
    float opacity = clamp(materials[mtl].opacity,0.0,1.0);                                      //Normalize material opacity
    if(materials[mtl].useOpacityTexture){                                                       //Surface opacity is derived from min of opacity texture color and material opacity, if set
        vec2 opacityUV = (uv * materials[mtl].opacityTextureUVScale);
        opacityUV += materials[mtl].opacityTextureScrollSpeed*timeSec;
        opacityUV += materials[mtl].opacityTextureOffset;
        vec4 opacityColor = texture(materials[mtl].opacityTexture, opacityUV);
        float opacityIntensity = opacityColor.a;
        if(!materials[mtl].opacityMasked){
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
    if(materials[mtl].useLighting){
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
    if(materials[mtl].useGammaCorrection){
        vec3 gammaCorrection = vec3(1.0/materials[mtl].gamma);
        fragColor = pow(linearColor, gammaCorrection);
    }
    
    gl_FragColor = vec4(fragColor, surfOpacity);
}