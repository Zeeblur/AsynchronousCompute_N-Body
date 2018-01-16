#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColour;
layout(location = 2) in vec2 inTexCoord;

layout(location = 3) in vec4 instancePos;

layout(location = 0) out vec3 fragColour;
layout(location = 1) out vec2 fragTexCoord;

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	vec3 v = instancePos.xyz;
	mat4 m = mat4(1.0);// vec4(inPos + instancePos.xyz, 1.0));
	m[3] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3];
	mat4 model = m * ubo.model;
    gl_Position = (ubo.proj * ubo.view * model) * vec4(inPos, 1.0);
    fragColour = instancePos.xyz;
	fragTexCoord = inTexCoord;
}