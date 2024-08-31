#version 460 core

in vec2 TexCoords;

out vec4 color;

layout (binding = 0, rgba32f) uniform image2D inputImage;

void main() {
	vec4 pixel = imageLoad(inputImage, ivec2(gl_FragCoord.xy));
	color = pixel;
}
