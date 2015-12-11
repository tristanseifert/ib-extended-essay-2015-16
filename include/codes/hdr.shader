// VERTEX
// A simple vertex shader to handle HDR.
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

// fragment output colour: this stores the bright parts of the image
layout (location = 0) out vec3 FragColour;

// This texture contains the contents of the scene.
uniform sampler2D texInColour;

float when_gt(float x, float y) {
	return max(sign(x - y), 0.0);
}

void main() {
	// sample the original buffer
	vec3 inColour = texture(texInColour, TexCoord).rgb;

	// copy the bright colours
	float brightness = dot(inColour.rgb, vec3(0.2126, 0.7152, 0.0722));
	FragColour = inColour.rgb * when_gt(brightness, 1.0);
}