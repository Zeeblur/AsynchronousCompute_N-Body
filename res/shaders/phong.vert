#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 3) in vec4 instancePos;

// binormals
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTang;


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
	normal = normalize(mat3(ubo.model) * inNormal);  // rotation matrix for transNormal into cameraspace
	

	// transform tangent & binormal to camera space
	tangent_out = normalize((mat3(ubo.model) * inTangent));
	//tangent_out = inTangent;

	//vec3 calcBi = cross (normal, tangent_out);
	//binormal_out = calcBi;

	binormal_out = normalize((mat3(ubo.model) * inBiTang));
	
	fragTexCoord = inTexCoord;
}