#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColour;
layout(location = 2) in vec2 inTexCoord;

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
	#use same ubo for every particle, inPos is vertex pos, + the instance position (from particle)
    gl_Position =  ubo.proj * ubo.view * ubo.model * (vec4(inPos, 1.0) + vec4(2.0f, 5.0f, 0.0f, 1.0f));
    fragColour = inColour;
	fragTexCoord = inTexCoord;
}