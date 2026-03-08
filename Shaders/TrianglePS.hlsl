struct VSOut
{
    float4 pos : SV_Position;
    float3 color : COLOR0;
};

float4 main(VSOut input) : SV_Target
{
    return float4(input.color, 1.0);
}