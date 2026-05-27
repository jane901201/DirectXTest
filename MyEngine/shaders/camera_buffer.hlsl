cbuffer CameraBuffer : register(b0)
{
    matrix View;
    matrix Projection;
    float3 CameraPos;
    float Padding;
};

VSOutput VSMain(VSpinput input) {
    VSOutput output;

    float4 worldPos = float4(input.position, 1.0f);
    float4 viewPos = mul(worldPos, View);
    output.position = mul(viewPos, Projection);

    return output;
}