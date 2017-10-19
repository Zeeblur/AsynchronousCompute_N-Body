#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColour;

layout(location = 0) out vec3 fragColour;

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
    gl_Position =  ubo.proj * ubo.view * ubo.model * vec4(inPos, 0.0, 1.0);
    fragColour = inColour;
}