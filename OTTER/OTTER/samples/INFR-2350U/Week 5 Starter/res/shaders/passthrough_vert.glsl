#version 410

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outPos;

uniform mat4 u_ModelViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;

void main()
{ 
	outUV = inUV;

	gl_Position = vec4(inPosition, 1.0);

	outPos = (vec4(inPosition, 1.0)).xyz;
}