#version 420

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inPos;

out vec4 frag_color;

layout (binding = 0) uniform sampler2D s_screenTex;
layout (binding = 1) uniform sampler2D s_DepthOfField;

void main()
{
	vec4 texCol1 = texture(s_screenTex, inUV);
	vec4 texCol2 = texture(s_DepthOfField, inUV);

	frag_color = mix(texCol1, texCol2, texCol2.a);
}