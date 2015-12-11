// VERTEX
// A simple vertex shader to handle lighting. It is meant to be rendered onto a
// single quad that fills the entire screen.
#version 400 core
layout (location = 0) in vec3 VtxPosition;
layout (location = 1) in vec2 VtxTexCoord;

out vec2 TexCoord;

void main() {
	gl_Position = vec4(VtxPosition, 1.0f);
	TexCoord = VtxTexCoord;
}

~~~
// FRAGMENT
// All lighting calculation is done in the fragment shader, pulling information
// from the different G buffer components, that were rendered by an earlier
// rendering pass: the position, normals, albedo and specular colours.
#version 400 core
in vec2 TexCoord;

// Output lighted colours.
layout (location = 0) out vec3 FragColour;

// These three textures are rendered into when the geometry is rendered.
uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

// Ambient light
struct AmbientLight {
	float Intensity;
	vec3 Colour;
};

uniform AmbientLight ambientLight;

// Directional, point and spotlight data
struct DirectionalLight {
	// direction pointing FROM light source
	vec3 Direction;

	vec3 DiffuseColour;
	vec3 SpecularColour;
};

const int NUM_DIRECTIONAL_LIGHTS = 4;
uniform DirectionalLight directionalLights[NUM_DIRECTIONAL_LIGHTS];

struct PointLight {
	vec3 Position;

	vec3 DiffuseColour;
	vec3 SpecularColour;
	float Linear;
	float Quadratic;
};

const int NUM_POINT_LIGHTS = 32;
uniform PointLight pointLights[NUM_POINT_LIGHTS];

struct SpotLight {
	vec3 Position;
	vec3 Direction;

	vec3 DiffuseColour;
	vec3 SpecularColour;
	float Linear;
	float Quadratic;
	
	// cosines of angles
	float InnerCutOff; // when the light begins to fade
	float OuterCutOff; // outside of this angle, no light is produced
};

const int NUM_SPOT_LIGHTS = 8;
uniform SpotLight spotLights[NUM_SPOT_LIGHTS];

// Number of directional, point and spotlights
uniform vec3 LightCount;
// General parameters
uniform vec3 viewPos;

// Inverse projection matrix: view space -> world space
uniform mat4 projMatrixInv;
// Inverse view matrix: clip space -> view space
uniform mat4 viewMatrixInv;
// Light view matrix: world space -> light space
uniform mat4 lightSpaceMatrix;

// Reconstructs the position from the depth buffer.
vec3 WorldPosFromDepth(float depth);

// 1.0 when x > y, 0.0 otherwise
float when_gt(float x, float y) {
	return max(sign(x - y), 0.0);
}

void main() {
	// Get the depth of the fragment and recalculate the position
	float Depth = texture(gDepth, TexCoord).x;
	vec3 FragWorldPos = WorldPosFromDepth(Depth);

	// retrieve the normals and shininess
	vec4 NormalShiny = texture(gNormal, TexCoord);
	vec3 Normal = NormalShiny.rgb;
	float Shininess = NormalShiny.a;
		
	// Calculate the view direction
	vec3 viewDir = normalize(viewPos - FragWorldPos);

	// retrieve albedo and specular
	vec4 AlbedoSpec = texture(gAlbedoSpec, TexCoord);
	vec3 Diffuse = AlbedoSpec.rgb;
	float Specular = AlbedoSpec.a;

	// if only the skybox is rendered at a given light, skip lighting
	if(Depth < 1.0) {
		// Ambient lighting
		vec3 ambient = Diffuse * ambientLight.Intensity;
		vec3 lighting = vec3(0, 0, 0);

		// Directional lights
		for(int i = 0; i < LightCount.x;  ++i) {
			// get some info about the light
			DirectionalLight light = directionalLights[i];
			vec3 lightDir = normalize(-light.Direction);

			// Diffuse
			vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.DiffuseColour;

			// Specular
			vec3 halfwayDir = normalize(lightDir + viewDir);
			float spec = pow(max(dot(Normal, halfwayDir), 0.0), Shininess);

			vec3 specular = spec * Specular * light.SpecularColour;
			
			// Output
			lighting += (diffuse + specular);
		}

		// Point lights
		for(int i = 0; i < LightCount.y; ++i) {
			// get some light info
			PointLight light = pointLights[i];
			vec3 lightDir = normalize(light.Position - FragWorldPos);

			// Diffuse
			vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.DiffuseColour;

			// Specular
			vec3 halfwayDir = normalize(lightDir + viewDir);
			float spec = pow(max(dot(Normal, halfwayDir), 0.0), Shininess);

			vec3 specular = spec * Specular * light.SpecularColour;

			// Attenuation
			float distance = length(light.Position - FragWorldPos);
			float attenuation = 1.0 / (1.0 + (light.Linear * distance) + 
								(light.Quadratic * distance * distance));
			
			diffuse *= attenuation;
			specular *= attenuation;

			// Output
			lighting += (diffuse + specular);
		}

		// Spotlights
		for(int i = 0; i < LightCount.z;  ++i) {
			// get some info about the light
			SpotLight light = spotLights[i];

			// Calculate to see whether we're inside the 'cone of influence'
			vec3 lightDir = normalize(light.Position - FragWorldPos);
			float theta = dot(lightDir, normalize(-light.Direction)); 

			// We're working with cosines, not angles, so >
			if(theta > light.OuterCutOff) {
				// Diffuse 
				vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.DiffuseColour;  

				// Specular
				vec3 halfwayDir = normalize(lightDir + viewDir);
				float spec = pow(max(dot(Normal, halfwayDir), 0.0), Shininess);

				vec3 specular = spec * Specular * light.SpecularColour;

				// Spotlight (soft edges)
				float theta = dot(lightDir, normalize(-light.Direction)); 
				float epsilon = (light.InnerCutOff - light.OuterCutOff);
				float intensity = clamp((theta - light.OuterCutOff) / epsilon, 0.0, 1.0);
				diffuse *= intensity;
				specular *= intensity;

				// Attenuation
				float distance = length(light.Position - FragWorldPos);
				float attenuation = 1.0f / (1.0 + (light.Linear * distance) + 
									(light.Quadratic * (distance * distance)));    

				diffuse  *= attenuation;
				specular *= attenuation; 

				lighting += (diffuse + specular);  
			}
		}

		// Combine everything
		lighting += ambient;
		// output colour of the fragment
		FragColour = lighting;
	}
}

// this is supposed to get the world position from the depth buffer
vec3 WorldPosFromDepth(float depth) {
	// Normalize Z
	float ViewZ = (depth * 2.0) - 1.0;
	// Get clip space
	vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, ViewZ, 1);
	// Clip space -> View space
	vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;
	// Perspective division
	viewSpacePosition /= viewSpacePosition.w;
	// View space -> World space
	vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;
	// Discard w component
	return worldSpacePosition.xyz;
}