#version 450

layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_color;

layout (location = 0) out vec3 v_color;

layout (binding = 0) uniform UboViewProjection
{
	mat4 projection;
	mat4 view;
} u_viewProjection;

layout (binding = 1) uniform UboModel
{
	mat4 model;
} u_model;

void main()
{
	gl_Position = u_viewProjection.projection * u_viewProjection.view * u_model.model * vec4(a_pos, 1.0);
	v_color = a_color;
}