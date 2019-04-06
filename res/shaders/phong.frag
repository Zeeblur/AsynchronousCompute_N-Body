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

void main()
{

	// Generate TBN matrix
	mat3 TBN = transpose(mat3(tangent, binormal, normal));  // camera space to tangent space

	// hard code lighting constants
	vec4 ambient_intensity = vec4(0.1);
	vec4 light_colour = vec4(1.0);
	vec3 light_dir = vec3(0, -1, 0); // camera space

	//vec3 light_dir = TBN * vec3(0, -1, 0); // tangent space
	
	vec4 emissive = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 diffuse_reflection = vec4(0.53, 0.45, 0.37, 1.0);
	vec4 specular_reflection = vec4(1.0);
	float shininess = 1.0;


	// get normal coordinate from texture
	vec3 samp_norm = texture(normalSampler, fragTexCoord).xyz;
	samp_norm = normalize((2.0 * samp_norm) - vec3(1.0));
	
	
	vec3 transN = normalize(samp_norm + normal);  // in tangent space
	
//	transN = TBN * samp_norm; // in cam space


	// calculate phong lighting
	vec4 ambient = diffuse_reflection * ambient_intensity;
		
	float dotD = clamp(dot(transN, light_dir), 0 , 1); // both in tangent space
	//float k = max(dotD, 0);

	vec4 diffuse = diffuse_reflection * light_colour * dotD;

	// Calculate view direction
	//vec3 view_dir = TBN * normalize(vec3(0, 0, 0)-position); // tangent space
	vec3 view_dir = normalize(vec3(0, 0, 0)-position); // camera space
	vec3 halfV = normalize(view_dir + light_dir);

	float dotS = clamp(dot(halfV, transN), 0, 1);
	//float kSpec = max(dotS, 0);

	vec4 specular = specular_reflection * light_colour * pow(dotS, shininess);
	
	vec4 primary = emissive + ambient + diffuse;

	outColor = texture(texSampler, fragTexCoord);
	outColor *= primary;
//	outColor += specular;

	//outColor = vec4(, 1.0);

	outColor.a = 1.0;
}