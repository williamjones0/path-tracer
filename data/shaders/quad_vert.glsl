#version 460 core

layout (location = 0) in vec3 aPos;

const vec2 quadVertices[6] = vec2[](
	vec2(-1.0,  1.0),
	vec2(-1.0, -1.0),
	vec2( 1.0, -1.0),

	vec2(-1.0,  1.0),
	vec2( 1.0, -1.0),
	vec2( 1.0,  1.0)
);

out vec2 TexCoords;

void main() {
	gl_Position = vec4(quadVertices[gl_VertexID], 0.0, 1.0);
	TexCoords = quadVertices[gl_VertexID] * 0.5 + 0.5;
}
