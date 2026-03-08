struct VSOut
{
    float4 pos : SV_Position;
    float3 color : COLOR0;
};

VSOut main(uint id : SV_VertexID)
{
    float2 p[3] = {
        float2(0.0, 0.6),
        float2(0.6, -0.6),
        float2(-0.6, -0.6)
    };

    float3 c[3] = {
        float3(1, 0, 0),
        float3(0, 1, 0),
        float3(0, 0, 1)
    };

    VSOut o;
    o.pos = float4(p[id], 0.0, 1.0);
    o.color = c[id];
    return o;
}