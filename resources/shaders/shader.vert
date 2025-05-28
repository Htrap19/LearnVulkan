#version 450

layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_color;

layout (location = 0) out vec3 v_color;

layout (binding = 0) uniform MVP
{
	mat4 projection;
	mat4 view;
	mat4 model;
} u_mvp;

void main()
{
	gl_Position = u_mvp.projection * u_mvp.view * u_mvp.model * vec4(a_pos, 1.0);
	v_color = a_color;
}