struct VSOutput
{
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
	float2 pos;
	if      (vertexID == 0) pos = float2(-3.0,  1.0);
	else if (vertexID == 1) pos = float2( 1.0,  1.0);
	else                    pos = float2( 1.0, -3.0);

	VSOutput output;
	output.position = float4(pos, 0.0, 1.0);
	output.texcoord = pos * 0.5 + 0.5;
	return output;
}
