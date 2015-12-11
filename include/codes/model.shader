// VERTEX
#version 400 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoords;

out vec3 WorldPos;
out vec2 TexCoords;
out vec3 Normal;

uniform mat4 model;
uniform mat4 projectionView; // projection * view
uniform mat3 normalMatrix; // transpose(inverse(mat3(model)))

void main() {
	// Forward the world position and texture coordinates
	vec4 worldPos = model * vec4(position, 1.0f);
	WorldPos = worldPos.xyz;
	TexCoords = texCoords;

	// Set position of the vertex pls
	gl_Position = projectionView * worldPos;

	// Send normals (multiplied by normal matrix)
	Normal = normalMatrix * normal;
}

~~~
// FRAGMENT
#version 400 core

// Material data
struct MaterialStruct {
	// how reflective the material is: lower value = more reflective
	float shininess;
};

// The normal/shininess buffer is colour attachment 0
layout (location = 0) out vec4 gNormal;
// The albedo/specular buffer is colour attachment 1
layout (location = 1) out vec4 gAlbedoSpec;

// Inputs from vertex shader
in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;

// Number of textures to sample (diffuse, specular)
uniform vec2 NumTextures;

// Samplers (for diffuse and specular)
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_diffuse2;
uniform sampler2D texture_diffuse3;
uniform sampler2D texture_diffuse4;

uniform sampler2D texture_specular1;
uniform sampler2D texture_specular2;
uniform sampler2D texture_specular3;
uniform sampler2D texture_specular4;

// Material data
uniform MaterialStruct Material;

// 1.0 when x == y, 0.0 otherwise
float when_eq(float x, float y) {
	return 1.0 - abs(sign(x - y));
}

// 1.0 when x < y; 0.0 otherwise
float when_lt(float x, float y) {
	return min(1.0 - sign(x - y), 1.0);
}

// 1.0 when x > y; 0.0 otherwise
float when_ge(float x, float y) {
	return 1.0 - when_lt(x, y);
}

void main() {
	// Store the per-fragment normals and material shininess into the gbuffer
	gNormal.rgb = normalize(Normal);
	gNormal.a = Material.shininess;

	// Diffuse per-fragment color
	vec3 diffuse = texture(texture_diffuse1, TexCoords).rgb;

	if(NumTextures.x >= 2) {
		diffuse += texture(texture_diffuse2, TexCoords).rgb;
	} if(NumTextures.x >= 3) {
		diffuse += texture(texture_diffuse3, TexCoords).rgb;
	} if(NumTextures.x >= 4) {
		diffuse += texture(texture_diffuse4, TexCoords).rgb;
	}

	// Specular intensity
	float specular = texture(texture_specular1, TexCoords).a;

	if(NumTextures.y >= 2) {
		specular += texture(texture_specular2, TexCoords).a;
	} if(NumTextures.y >= 3) {
		specular += texture(texture_specular3, TexCoords).a;
	} if(NumTextures.y >= 4) {
		specular += texture(texture_specular4, TexCoords).a;
	}

	// store diffuse colour and specular component
	gAlbedoSpec.rgb = diffuse;
	gAlbedoSpec.a = specular;
}