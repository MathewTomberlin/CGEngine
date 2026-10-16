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
    vec2 normalTextureUVScale;
    float normalStrength;
    int useNormalTexture;
    float metallic;
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
    sampler2D normalTexture;
};
uniform MaterialTextures materialTextures[8];

in vec2 uv;
in vec3 normal;
in vec3 vertex;
in float matId;
in vec4 vertexColour;
in vec3 vertexGlow;
in float vertexSky;
in float vertexFlicker;     //The phase of a wavering flame (a placed torch), 0 for every other surface
flat in vec3 vertexTiles;
in vec4 vertexSplat0;	//the splat weights, channels 0 to 3 (see VoxelVertex::splat)
in vec4 vertexSplat1;	//channels 4 to 7
in vec4 vertexSplat2;	//channels 8 to 11
in vec4 vertexSplat3;	//channels 12 to 15
in float vertexDamage;      //How much of the voxel's block has been hit, 0 to 1 (see crackMask)

out vec4 FragColor;

//Voxel meshes set triplanar: the diffuse texture is projected along the three world axes and blended by how square the
//surface faces each axis. One texel is then one block on every face, so slopes and walls are not stretched the way a single
//planar projection stretches them.
uniform int triplanar;

//The tile array (Renderer::setVoxelTiles): one layer per block face, sampled in world space like the texture above. Tiles are
//repeated once per block, so a block's tile reads the same on every face it shows.
uniform int tiled;
uniform sampler2DArray voxelTiles;
//The splat blocks' tile layers (top, sides, bottom), by channel: Renderer::setVoxelSplatTiles.
uniform vec3 splatTiles[16];

// The splat weight of channel k (0 to 15).
float splatWeight(int k){
    vec4 group = k < 4 ? vertexSplat0 : (k < 8 ? vertexSplat1 : (k < 12 ? vertexSplat2 : vertexSplat3));
    return group[k % 4];
}
vec4 triplanarTile(vec3 pos, vec3 n, float layer){
    vec3 weights = pow(abs(n), vec3(4.0));
    weights /= weights.x + weights.y + weights.z + 1e-6;
    vec4 alongX = texture(voxelTiles, vec3(pos.zy, layer));
    vec4 alongY = texture(voxelTiles, vec3(pos.xz, layer));
    vec4 alongZ = texture(voxelTiles, vec3(pos.xy, layer));
    return alongX * weights.x + alongY * weights.y + alongZ * weights.z;
}
//One block's tile set (its top, sides and bottom layers), blended by the normal: the top tile where the surface looks up, the
//bottom tile where it looks down, the side tile between. So a slope passes from one tile to another across its surface.
vec4 tileColour(vec3 tiles, vec3 n, vec3 pos){
    float up = smoothstep(0.1, 0.9, n.y);
    float down = smoothstep(0.1, 0.9, -n.y);
    float side = max(0.0, 1.0 - up - down);
    //Along a side the top tile fades in by height, over the top and (more weakly) the bottom of each block's row, so a grass top
    //runs down a cliff in a soft fade instead of ending on a line where the normal turns. The three weights still sum to 1.
    //The bottom of a side fades toward the top tile too, more weakly: there the side meets the ground below it.
    float along = fract(pos.y - 0.2);
    float fadeTop = smoothstep(0.3, 1.0, along);
    float fadeBottom = 0.7 * (1.0 - smoothstep(0.0, 0.3, along));
    float fade = max(fadeTop, fadeBottom) * side;
    vec4 c = vec4(0.0);
    if(up + fade > 0.0) c += (up + fade) * triplanarTile(pos, n, floor(tiles.x + 0.5));
    if(side - fade > 0.0) c += (side - fade) * triplanarTile(pos, n, floor(tiles.y + 0.5));
    if(down > 0.0) c += down * triplanarTile(pos, n, floor(tiles.z + 0.5));
    return c;
}

vec4 triplanarSample(sampler2D tex, vec3 pos, vec3 n){
    vec3 weights = pow(abs(n), vec3(4.0));
    weights /= weights.x + weights.y + weights.z + 1e-6;
    vec4 alongX = texture(tex, pos.zy);
    vec4 alongY = texture(tex, pos.xz);
    vec4 alongZ = texture(tex, pos.xy);
    return alongX * weights.x + alongY * weights.y + alongZ * weights.z;
}

//Cracks on a damaged voxel: the borders between the cells of a Voronoi pattern in world space, which read as the thin branching
//lines of a broken surface. The lines widen with the damage, so a block gets more and wider cracks as it is hit.
float latticeHash(vec3 p){ return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float crackEdge(vec3 p){
    vec3 cell = floor(p);
    vec3 local = fract(p);
    float nearest = 8.0;
    float second = 8.0;
    for(int z = -1; z <= 1; z++){
        for(int y = -1; y <= 1; y++){
            for(int x = -1; x <= 1; x++){
                vec3 g = vec3(float(x), float(y), float(z));
                vec3 at = cell + g;
                vec3 jitter = vec3(latticeHash(at), latticeHash(at + vec3(19.1, 7.3, 3.7)), latticeHash(at + vec3(5.2, 13.9, 27.4)));
                vec3 r = g + jitter - local;
                float d = dot(r, r);
                if(d < nearest){ second = nearest; nearest = d; }
                else if(d < second){ second = d; }
            }
        }
    }
    return sqrt(second) - sqrt(nearest);                 // 0 on a border between two cells
}
float crackMask(vec3 p, float damage){
    float edge = crackEdge(p * 2.4);
    float width = mix(0.04, 0.10, clamp(damage, 0.0, 1.0));   // more hits, wider cracks (in cell units)
    return 1.0 - smoothstep(width * 0.5, width, edge);
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
    //Fade to lit over the last part of the map, so the map's square edge is not a hard line.
    float edge = min(min(uvz.x, 1.0 - uvz.x), min(uvz.y, 1.0 - uvz.y));
    float fade = smoothstep(0.0, 0.06, edge);
    float bias = max(0.0006 * (1.0 - dot(normal, towardsLight)), 0.0002);
    float lit = 0.0;
    for(int x = -1; x <= 1; ++x){
        for(int y = -1; y <= 1; ++y){
            float depth = texture(shadowMap, uvz.xy + vec2(x, y) * shadowTexel).r;
            lit += (uvz.z - bias > depth) ? 0.0 : 1.0;
        }
    }
    return mix(1.0, lit / 9.0, fade);
}

//A tangent-space normal map (roadmap 35.1). The tangent frame comes from the screen derivatives of the position and the uv, so a
//mesh needs no tangents: the map's blue channel points out of the surface, its red and green tilt the normal, and strength scales the tilt.
vec3 perturbNormal(vec3 n, vec3 p, vec2 texUV, sampler2D map, float strength){
    vec3 mapN = texture(map, texUV).xyz * 2.0 - 1.0;
    mapN.xy *= strength;
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(texUV);
    vec2 duv2 = dFdy(texUV);
    vec3 dp2perp = cross(dp2, n);
    vec3 dp1perp = cross(n, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-12));
    return normalize(mat3(T * invmax, B * invmax, n) * mapN);
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
    //A metal's highlight takes the surface's colour (metallic 1); a dielectric's stays the material's own (metallic 0).
    vec3 specularColor = mix(materials[mtl].specularColor.rgb, surfColor.rgb, materials[mtl].metallic);
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
    //A voxel surface of one block (the water, the wood and the leaves) takes that block's tiles; the terrain's smooth skin blends
    //the splat blocks' tiles by its splat weights, which change smoothly across each triangle.
    float splatSum = dot(vertexSplat0 + vertexSplat1 + vertexSplat2 + vertexSplat3, vec4(1.0));
    if(tiled != 0 && triplanar != 0 && vertexTiles.x < 254.5){
        surfColor = tileColour(vertexTiles, normalize(normal), surfPos);
    } else if(tiled != 0 && triplanar != 0 && splatSum > 0.5){
        vec3 n = normalize(normal);
        vec4 blended = vec4(0.0);
        for(int k = 0; k < 16; k++){
            float w = splatWeight(k);
            if(w > 0.01) blended += w * tileColour(splatTiles[k], n, surfPos);
        }
        surfColor = blended / splatSum;
    } else if(materials[mtl].useDiffuseTexture != 0){                                                 //Surface color is derived from diffuse texture color * material diffuse color, if set
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
    surfColor.rgb *= vertexColour.rgb;                                                          //Per-vertex tint (white unless the mesh sets it)
    //Cracks: a damaged voxel gets dark lines on its surface (the damage is 0 for every other mesh). The damage is interpolated
    //across the triangles, so a triangle that reaches a hit block from a clean one fades its cracks out (the envelope).
    if(triplanar != 0 && vertexDamage > 0.0) surfColor.rgb *= 1.0 - 0.6 * crackMask(surfPos, vertexDamage) * smoothstep(0.2, 0.5, vertexDamage);

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
    //Voxel materials can be see-through (the water): their opacity multiplies the surface's.
    if(triplanar != 0 && materials[mtl].opacity > 0.0) surfOpacity *= opacity;
    surfOpacity *= vertexColour.a;                                                              //A vertex's own opacity (1 unless a ghost fades it)
    if(surfOpacity < materials[mtl].alphaCutoff) discard;                                       //Discard fragments with alpha below the cutoff

    //Surface lighting
    //Unlit materials show their surface colour. Lit ones get only what the lights give (ambient fill + direct light);
    //adding the full unlit colour as well made every lit surface at least as bright as its texture and overexposed white ones.
    vec3 linearColor = surfColor.rgb;
    //The sun and the ambient light reach a voxel only as far as the sky does, so a sealed cave gets none at any hour. Other
    //meshes are not voxels and get all of it.
    float skyVisible = triplanar != 0 ? vertexSky : 1.0;
    if(materials[mtl].useLighting != 0){
        linearColor = vec3(0.0);
        vec3 surfNormal = normalize(normal);
        if(materials[mtl].useNormalTexture != 0 && triplanar == 0){
            surfNormal = perturbNormal(surfNormal, surfPos, uv * materials[mtl].normalTextureUVScale, materialTextures[mtl].normalTexture, materials[mtl].normalStrength);
        }
        //Voxel skins are closed surfaces drawn from both sides: seen from behind, the face is lit as the side the viewer is on.
        if(triplanar != 0 && !gl_FrontFacing) surfNormal = -surfNormal;
        vec3 surfViewDir = normalize(cameraPosition - surfPos);

        for(int i = 0; i < lightCount; ++i){
            if(lights[i].brightness > 0){
                //A point light (a held torch) is not shaded by the sky: it lights the cave the player is in.
                float gate = lights[i].position.w == 0.0 ? skyVisible : 1.0;
                linearColor += applyLight(lights[i], surfColor.rgb * gate, surfNormal, surfPos, surfViewDir, mtl);
            }
        }
    }

	//A voxel surface that neither the sun nor a lamp reaches is not black: it keeps a faint share of its own tile, so a dark tunnel's
    //walls still show their texture. The sun's share of the light is what the sky lets through (skyVisible), so this fills the rest.
    const float CaveFill = 0.12;
    if(triplanar != 0 && materials[mtl].useLighting != 0) linearColor += surfColor.rgb * CaveFill * (1.0 - skyVisible);

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
    
    //Emission: a light block's own glow, added after the lighting so the sun and the shadows do not dim it. Fog still applies.
    //A torch's flame wavers: its glow is scaled by two slow sines, each torch on its own phase (vertexFlicker is 0 elsewhere).
    float flameWaver = vertexFlicker > 0.0 ? 0.88 + 0.12 * sin(timeSec * 7.3 + vertexFlicker * 6.283) * sin(timeSec * 2.9 + vertexFlicker * 3.7) : 1.0;
    //A block's glow and the light of nearby lamps are its own texture lit up, so a lamp-lit wall or a lava surface shows its tiles.
    //A torch's flame (flicker above 0) is a plain colour. EmissiveGain brings the lit tiles up to a bright glow in a cave.
    const float EmissiveGain = 1.5;
    vec3 emitted = vertexFlicker > 0.0 ? vertexGlow * flameWaver : vertexGlow * surfColor.rgb * EmissiveGain;
    fragColor += emitted;

    //Distance fog: fades the surface into the fog colour between the start and end distances
    if(fogRange.y > fogRange.x){
        float fogAmount = clamp((length(surfPos - cameraPosition) - fogRange.x) / (fogRange.y - fogRange.x), 0.0, 1.0);
        fragColor = mix(fragColor, fogColour, fogAmount);
    }

    FragColor = vec4(fragColor, surfOpacity);
}