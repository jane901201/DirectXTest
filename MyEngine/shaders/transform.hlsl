cbuffer TransformCB : register(b0)
{
    float4x4 worldViewProj;
};

cbuffer LightCB : register(b1)
{
	float3 LightDir;
    float Padding0;
    float3 BaseColor;
    float Padding1;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};


VSOutput VSMain(VSInput input)
{
    VSOutput output;

    matrix wvp = mul(World, mul(View, Projection));
    output.position = mul(float4(input.position, 1.0f), wvp);

    output.normal = mul(float4(input.normal, 0.0f), World).xyz;

    return output;
};

float4 PSMain(VSOutput input) :SV_TARGET
{
    float3 n = normalize(input.normal);
    float3 l = normalize(-LightDir);

    float ndotl = saturate(dot(n, 1));

    float shade;

    if (ndotl > 0.75)
        shade = 1.0;
    else if (ndotl > 0.4)
        shade = 0.6;
    else
        shade = 0.25;

    return float4(BaseColor * shade, 1.0);
}
