// VERTEX
// A simple vertex shader to handle bloom.
#version 400 core
layout (location = 0) in vec3 VtxPosition;
layout (location = 1) in vec2 VtxTexCoord;

out vec2 TexCoords;

void main() {
	gl_Position = vec4(VtxPosition, 1.0f);
	TexCoords = VtxTexCoord;
}

~~~
// FRAGMENT
#version 400 core

in vec2 TexCoords;

// output the actual colour
layout (location = 0) out vec4 FragColour;

// Raw scene colour output
uniform sampler2D inSceneColours;
// Brightest parts of the scene (pre-blurred, low res)
uniform sampler2D inBloomBlur;

// white point: the brightest colour
uniform vec3 whitePoint;
// exposure value
uniform float exposure;

vec3 TonemapColour(vec3 x);

void main() {
	// additively blend the HDR and bloom textures
	vec3 hdrColour = texture(inSceneColours, TexCoords).rgb;      
	vec3 bloomColour = texture(inBloomBlur, TexCoords).rgb;
	hdrColour += bloomColour;

	// apply HDR and white point
	hdrColour = TonemapColour(hdrColour * exposure);
	hdrColour = hdrColour / TonemapColour(whitePoint);

	// compute luma for the FXAA shader
	vec4 outColour = vec4(hdrColour, 0);
  	outColour.a = dot(outColour.rgb, vec3(0.299, 0.587, 0.114));

	FragColour = outColour;
}

// Applies the tonemapping algorithm on a colour
vec3 TonemapColour(vec3 x) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;

	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}