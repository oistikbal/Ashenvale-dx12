struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

[RootSignature("")]
VSOutput vs_main(uint vertexId : SV_VertexID)
{
    float2 positions[3] =
    {
        float2(0.0f, 0.5f),
        float2(0.5f, -0.5f),
        float2(-0.5f, -0.5f) 
    };

    float3 colors[3] =
    {
        float3(1, 0, 0),
        float3(0, 1, 0), 
        float3(0, 0, 1) 
    };

    VSOutput output;
    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.color = colors[vertexId];
    return output;
}

[RootSignature("")]
float4 ps_main(VSOutput input) : SV_Target
{
    return float4(input.color, 1.0f);
}
