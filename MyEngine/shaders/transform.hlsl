cbuffer TransformCB : register(b0)
{
    float4x4 worldViewProj;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    output.uv = input.uv;
    return output;
}