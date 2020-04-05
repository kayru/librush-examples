cbuffer SceneConstants : register(b0)
{
	float4x4 g_matViewProj;
};

cbuffer InstanceConstants : register(b1)
{
	row_major float4x4 g_matWorld;
};

void main(
	in float3 a_pos0 : POSITION0, 
	in float4 a_col0 : COLOR0,
	out float4 v_position : SV_Position,
	out float4 v_color : COLOR0) 
{
	float3 worldPos = (mul(float4(a_pos0, 1), g_matWorld)).xyz;
	v_position = mul(float4(worldPos, 1), g_matViewProj);
	v_color = a_col0;
}
