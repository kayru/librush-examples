RWStructuredBuffer<uint> outputA : register(u0, space0);
RWBuffer<uint> outputB : register(u1, space0);

[numthreads(1, 1, 1)]
void main()
{
	outputA[0] = 0xAAAAAAAAu;
	outputB[0] = 0xBBBBBBBBu;
}
