struct VSOutput
{
	float4 pos : SV_Position;
};

float4 main(float2 pos : POSITION) : SV_Position
{
	return float4(pos, 0.0, 1.0);
}
