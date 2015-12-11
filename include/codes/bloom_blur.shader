// VERTEX
// A simple vertex shader to handle the blur for bloom.
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
#version 400 core

in vec2 TexCoord;

// output the actual colour
layout (location = 0) out vec4 FragColour;

// The input from the last pass of the shader
uniform sampler2D inTex;

// The direction of the blur: (1, 0) for horizontal, (0, 1) for vertical.
uniform vec2 direction;
// Size of texture
uniform vec2 resolution;

// Performs a 13x13 Gaussian blur.
vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction);

void main() {
	FragColour = blur13(inTex, TexCoord, resolution, direction);
}

// Performs a 13x13 Gaussian blur.
vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
	vec4 color = vec4(0.0);
	
	vec2 off1 = vec2(1.411764705882353) * direction;
	vec2 off2 = vec2(3.2941176470588234) * direction;
	vec2 off3 = vec2(5.176470588235294) * direction;

	color += texture(image, uv) * 0.1964825501511404;
	color += texture(image, uv + (off1 / resolution)) * 0.2969069646728344;
	color += texture(image, uv - (off1 / resolution)) * 0.2969069646728344;
	color += texture(image, uv + (off2 / resolution)) * 0.09447039785044732;
	color += texture(image, uv - (off2 / resolution)) * 0.09447039785044732;
	color += texture(image, uv + (off3 / resolution)) * 0.010381362401148057;
	color += texture(image, uv - (off3 / resolution)) * 0.010381362401148057;

	return color;
}