#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

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
		
	float dotD = dot(normal, light_dir);
	float k = max(dotD, 0);

	vec4 diffuse = diffuse_reflection * light_colour * k;

	// Calculate view direction
	vec3 view_dir = normalize(vec3(0, 0, 0)-position);
	vec3 halfV = normalize(view_dir + light_dir);

	float dotS = dot(halfV, transN);
	float kSpec = max(dotS, 0);

	vec4 specular = specular_reflection * light_colour * pow(kSpec, shininess);
	
	vec4 primary = emissive + ambient + diffuse;

	outColor = texture(texSampler, fragTexCoord);
	outColor *= primary;
	outColor += specular;

	outColor.a = 1.0;
}