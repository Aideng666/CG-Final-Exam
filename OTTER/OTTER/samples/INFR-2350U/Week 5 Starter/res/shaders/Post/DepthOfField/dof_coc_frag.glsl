#version 420

layout(location = 0) in vec3 inPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

out vec4 frag_color;

layout (binding = 0) uniform sampler2D s_screenTex;

uniform float u_Aperture;
uniform float u_PlaneInFocus;
uniform float u_FocalLength;
uniform vec3 u_LightPos;

uniform vec3 u_CamPos;

float maxCoc = 1.0;

float CircleOfConfusion()
{

	float S2 = abs(distance(inPos, u_CamPos));

	float coc = u_Aperture * (abs(S2 - u_PlaneInFocus) / S2) * (u_FocalLength / (u_PlaneInFocus - u_FocalLength));

	float sensorHeight = 0.024f; // 24 mm

    float sensorPercentage =    coc / sensorHeight;

    float blur = clamp(sensorPercentage, 0.0f, maxCoc);

    return blur;
}

vec3 CalcPixelColor(vec4 color)
{
    vec3 L = normalize(u_LightPos - inPos);
    vec3 N = normalize(inNormal);

    float diffuse = max(0.0, dot(N, L));

    if (diffuse <= 0.00) diffuse = 0.00;
    else if (diffuse <= 0.25) diffuse = 0.25;
    else if (diffuse <= 0.50) diffuse = 0.50;
    else if (diffuse <= 0.75) diffuse = 0.75;
    else diffuse = 1.00;

    return vec3(0.5, 0.5, 0.5) * (diffuse * 0.8f) + color.rgb;
}

float linearize_depth(float d, float zNear, float zFar)
{
    float z_n = 2.0 * d - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));
}

void main()
{
	vec4 texCol = texture(s_screenTex, inUV);

    frag_color.rgb = CalcPixelColor(texCol);

    frag_color.a = CircleOfConfusion();

}