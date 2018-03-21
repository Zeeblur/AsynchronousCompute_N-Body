#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform sampler2D normalSampler;


layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 binormal;

layout(location = 0) out vec4 outColor;

// forward declare method
vec3 calc_normal(in vec3 normal, in vec3 tangent, in vec3 binormal, in vec3 sampled_normal, in vec2 tex_coord)
{
	// ****************************************************************
	// Ensure normal, tangent and binormal are unit length (normalized)
	// ****************************************************************

	normal = normalize(normal);
	tangent = normalize(tangent);
	binormal = normalize(binormal);
	
	sampled_normal = (2.0 * sampled_normal) - vec3(1.0, 1.0, 1.0);
	
	// *******************
	// Generate TBN matrix
	// *******************
	mat3 TBN = mat3(tangent.x, tangent.y, tangent.z, binormal.x, binormal.y, binormal.z, normal.x, normal.y, normal.z);
	
	// ****************************************
	// Return sampled normal transformed by TBN
	// - remember to normalize
	// ****************************************
	vec3 trans_normal = TBN * sampled_normal;
	trans_normal = normalize(trans_normal);

	return trans_normal; // Change!!!
}

void main()
{
	// hard code lighting constants
	vec4 ambient_intensity = vec4(0.1);
	vec4 light_colour = vec4(1.0);
	vec3 light_dir = vec3(0, -1, 0);
	
	
	vec4 emissive = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 diffuse_reflection = vec4(0.53, 0.45, 0.37, 1.0);
	vec4 specular_reflection = vec4(1.0);
	float shininess = 1.0;

	// calculate phong lighting
	vec4 ambient = diffuse_reflection * ambient_intensity;
	

	// Calculate view direction
	vec3 view_dir = normalize(-position);
	vec3 halfV = normalize(view_dir + light_dir);

	// normal mapping
	vec3 samp_norm = texture(normalSampler, fragTexCoord).xyz;
	vec3 transN = calc_normal(normal, tangent, binormal, samp_norm, fragTexCoord);
	
	float dotS = dot(halfV, transN);
	float kSpec = max(dotS, 0);
	
	float dotD = dot(transN, light_dir);
	float k = max(dotD, 0);
	vec4 diffuse = diffuse_reflection * light_colour * k;
	
	vec4 specular = specular_reflection * light_colour * pow(kSpec, shininess);

	vec4 primary = emissive + ambient + diffuse;

	outColor = texture(texSampler, fragTexCoord);
	outColor *= primary;
	outColor += specular;
	
	//outColor = vec4(transN, 1.0);

	outColor.a = 1.0;
}