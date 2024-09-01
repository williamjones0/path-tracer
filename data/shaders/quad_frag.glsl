#version 460 core

in vec2 TexCoords;

out vec4 color;

layout (binding = 0, rgba32f) uniform image2D inputImage;

void main() {
	vec4 pixel = imageLoad(inputImage, ivec2(gl_FragCoord.xy));

	float gamma = 2.2;
	color = pixel; // vec4(pow(pixel.rgb, vec3(1.0 / gamma)), 1.0);
}
