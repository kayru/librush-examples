#version 450

layout (location = 0) out vec2 texcoord;

void main()
{
	vec3 pos;

	if      (gl_VertexIndex == 0) pos = vec3(-3.0,  1.0, 0.0);
	else if (gl_VertexIndex == 1) pos = vec3( 1.0,  1.0, 0.0);
	else                          pos = vec3( 1.0, -3.0, 0.0);

	gl_Position = vec4(pos, 1.0);

	texcoord = vec2(pos.x, pos.y) * 0.5 + 0.5;
}
