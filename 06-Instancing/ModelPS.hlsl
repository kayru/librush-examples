void main(
	in float4 v_position : SV_Position,
	in float4 v_color : COLOR0,
	out float4 fragColor : SV_Target)
{
	fragColor = v_color;
}
