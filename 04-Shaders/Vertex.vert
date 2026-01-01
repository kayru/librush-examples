#version 450

layout (location = 0) in vec2 a_pos0;

void main()
{
	gl_Position = vec4(a_pos0, 0.0, 1.0);
}
