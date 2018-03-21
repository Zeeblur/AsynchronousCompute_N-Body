#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 3) in vec4 instancePos;

layout(location = 0) out vec3 position;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 tangent_out;
layout(location = 4) out vec3 binormal_out;

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
	// transformation matrix
	m[3] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3];
	
	// scale matrix
	mat4 s = mat4(1.0);
	s[0] = s[0] * instancePos.w;
	s[1] = s[1] * instancePos.w;
	s[2] = s[2] * instancePos.w;
	
	mat4 trs = m * s;
	mat4 model = trs * ubo.model;
	gl_Position = (ubo.proj * ubo.view * model) * vec4(inPos, 1.0);
	
    position = vec3(model * vec4(inPos, 1.0));
	normal = mat3(ubo.model) * inNormal;  // rotation matrix for transNormal
	normal = normalize(normal);
	
	// calculate tangent and binormal
	vec3 c1 = cross(inNormal, vec3(0, 0, 1));
	vec3 c2 = cross(inNormal, vec3(0, 1, 0));
	
	vec3 calcTang = vec3(0);
	vec3 calcBi = vec3(0);
    
	if (length(c1) > length(c2))
		calcTang = normalize(c1);
	else
		calcTang = normalize(c2);
    
	calcBi = normalize(cross(inNormal, calcTang));

	// transform tangent & binormal
	tangent_out = mat3(ubo.model) * calcTang;
	binormal_out = mat3(ubo.model) * calcBi;
	
	fragTexCoord = inTexCoord;
}