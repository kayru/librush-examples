cbuffer TonemapConstants : register(b0, space0)
{
	float exposure;
	float gamma;
};

SamplerState linearClampSampler : register(s1, space0);
Texture2D inputTexture : register(t2, space0);

// Tonemapping implementation by Tomasz Stachowiak (MIT license)
// https://github.com/h3r2tic/rtoy-samples/blob/4c76e7efadb47eae5a290e11447815300dbe4131/assets/shaders/tonemap_sharpen.glsl

// Rec. 709
float calculate_luma(float3 col)
{
	return dot(float3(0.2126, 0.7152, 0.0722), col);
}

float tonemap_curve(float v)
{
	float c = v + v*v + 0.5*v*v*v;
	return c / (1.0 + c);
}

float3 tonemap_curve3(float3 v)
{
	return float3(tonemap_curve(v.r), tonemap_curve(v.g), tonemap_curve(v.b));
}

float3 neutral_tonemap(float3 col)
{
	// GLSL mat3 scalar ctor is column-major; transposed to HLSL row-major float3x3
	float3x3 ycbr_mat = float3x3(
		0.2126, -0.1146,  0.5,
		0.7152, -0.3854, -0.4542,
		0.0722,  0.5,   -0.0458);
	float3 ycbcr = mul(col, ycbr_mat);

	float chroma = length(ycbcr.yz) * 2.4;
	float bt = tonemap_curve(chroma);

	float desat = max((bt - 0.7) * 0.8, 0.0);
	desat *= desat;

	float3 desat_col = lerp(col.rgb, ycbcr.xxx, desat);

	float tm_luma = tonemap_curve(ycbcr.x);
	float3 tm0 = col.rgb * max(0.0, tm_luma / max(1e-5, calculate_luma(col.rgb)));
	float final_mult = 0.97;
	float3 tm1 = tonemap_curve3(desat_col);

	col = lerp(tm0, tm1, bt * bt);

	return col * final_mult;
}

float4 main(float2 texcoord : TEXCOORD0) : SV_Target
{
	float3 color = inputTexture.Sample(linearClampSampler, texcoord).rgb;

	color = neutral_tonemap(color * exposure);
	color.x = pow(color.x, 1.0 / gamma);
	color.y = pow(color.y, 1.0 / gamma);
	color.z = pow(color.z, 1.0 / gamma);

	return float4(color, 1);
}
