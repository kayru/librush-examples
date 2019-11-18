#version 450

layout (location = 0) in vec2 texcoord;
layout (location = 0) out vec4 fragColor;

layout (binding = 0) uniform sampler linearClampSampler;
layout (binding = 1) uniform texture2D inputTexture;

// Tonemapping implementation by Tomasz Stachowiak (MIT license)
// https://github.com/h3r2tic/rtoy-samples/blob/4c76e7efadb47eae5a290e11447815300dbe4131/assets/shaders/tonemap_sharpen.glsl

// Rec. 709
float calculate_luma(vec3 col)
{
	return dot(vec3(0.2126, 0.7152, 0.0722), col);
}

float tonemap_curve(float v)
{
	// Similar in shape, but more linear (less compression) in the mids
	float c = v + v*v + 0.5*v*v*v;
	return c / (1.0 + c);
}

vec3 tonemap_curve(vec3 v)
{
	return vec3(tonemap_curve(v.r), tonemap_curve(v.g), tonemap_curve(v.b));
}

vec3 neutral_tonemap(vec3 col)
{
	mat3 ycbr_mat = mat3(.2126, .7152, .0722, -.1146,-.3854, .5, .5,-.4542,-.0458);
	vec3 ycbcr = col * ycbr_mat;

	float chroma = length(ycbcr.yz) * 2.4;
	float bt = tonemap_curve(chroma);

	float desat = max((bt - 0.7) * 0.8, 0.0);
	desat *= desat;

	vec3 desat_col = mix(col.rgb, ycbcr.xxx, desat);

	float tm_luma = tonemap_curve(ycbcr.x);
	vec3 tm0 = col.rgb * max(0.0, tm_luma / max(1e-5, calculate_luma(col.rgb)));
	float final_mult = 0.97;
	vec3 tm1 = tonemap_curve(desat_col);

	col = mix(tm0, tm1, bt * bt);

	return col * final_mult;
}

void main()
{
	vec3 color = texture(sampler2D(inputTexture, linearClampSampler), texcoord).rgb;

	float exposure = 1;
	float gamma = 1;

	color = neutral_tonemap(color * exposure);
	color.x = pow(color.x, 1.0 / gamma);
	color.y = pow(color.y, 1.0 / gamma);
	color.z = pow(color.z, 1.0 / gamma);

	fragColor = vec4(color, 1);
}
