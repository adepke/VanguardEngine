struct Input
{
	float3 Position : POSITION;
	float4 Color : COLOR;
};

struct Output
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

Output main(Input In)
{
	Output Out;
	Out.Position = float4(In.Position.x, In.Position.y, In.Position.z, 1.f);
	Out.Color = In.Color;

	return Out;
}