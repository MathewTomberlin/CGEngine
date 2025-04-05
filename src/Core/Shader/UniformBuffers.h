namespace CGEngine {
	struct MaterialUBO {
		static const int MAX_MATERIALS = 8;		//Max number of materials
		struct Material {
			glm::vec4 diffuseColor;					//16
			glm::vec4 specularColor;				//32
			glm::vec2 diffuseTextureUVScale;		//40
			glm::vec2 diffuseTextureScrollSpeed;	//48
			glm::vec2 diffuseTextureOffset;			//56
			glm::vec2 specularTextureUVScale;		//64
			glm::vec2 specularTextureScrollSpeed;	//72
			glm::vec2 specularTextureOffset;		//80
			glm::vec2 opacityTextureUVScale;		//88
			glm::vec2 opacityTextureScrollSpeed;	//96
			glm::vec2 opacityTextureOffset;			//104
			float smoothnessFactor;					//108
			float opacity;							//112
			float alphaCutoff;						//116
			float gamma;							//120
			int useDiffuseTexture;					//124
			int useSpecularTexture;					//128
			int useOpacityTexture;					//132
			int useLighting;						//136
			int useGammaCorrection;					//140
			int opacityMasked;						//144
		} materials[MAX_MATERIALS];
	};

	struct LightUBO {
		static const int MAX_LIGHTS = 10;		//Max number of lights
		struct Light {
			glm::vec4 position;					//16
			glm::vec4 intensities;				//32
			glm::vec4 lightDirection;			//48
			float brightness;					//52
			float attenuation;					//56
			float ambiance;						//60
			float coneAngle;					//64
		} lights[MAX_LIGHTS];
		int lightCount;							//68
		int padding1; 							//72
		int padding2; 							//76
		int padding3; 							//80
	};

	struct BoneUBO {
		static const int MAX_BONES = 100;		//Max number of bones
		glm::mat4 boneMatrices[MAX_BONES];		//16
		int boneCount;							//20
		int padding1; 							//24
		int padding2; 							//28
		int padding3;							//32
	};

	struct TransformUBO {
		glm::mat4 camera;						//16
		glm::mat4 model;						//32
	};
}