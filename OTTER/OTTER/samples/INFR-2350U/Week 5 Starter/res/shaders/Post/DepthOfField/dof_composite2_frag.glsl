#version 420

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inPos;

out vec4 frag_color;

layout (binding = 0) uniform sampler2D s_colorTex;
layout (binding = 1) uniform sampler2D s_depthTex;

void main()
{
	vec4 texCol1 = texture(s_colorTex, inUV);
	vec4 texCol2 = texture(s_depthTex, inUV);


	frag_color = vec4(texCol1.rgb, texCol2.r);
}