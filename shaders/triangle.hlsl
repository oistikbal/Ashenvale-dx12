#define RS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | SAMPLER_HEAP_DIRECTLY_INDEXED | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED )"

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD2;
};

[RootSignature(RS)]
VSOutput vs_main(uint vertexId : SV_VertexID)
{
    float2 positions[3] =
    {
        float2(0.0f, 0.5f),
        float2(0.5f, -0.5f),
        float2(-0.5f, -0.5f)
    };

    float2 uv[3] =
    {
        float2(0.0f, 0.5f),
        float2(0.5f, -0.5f),
        float2(-0.5f, -0.5f)
    };

    VSOutput output;
    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.uv = uv[vertexId];
    return output;
}

float4 ps_main(VSOutput input) : SV_Target
{
    
    Texture2D<float4> tex = ResourceDescriptorHeap[0]; 
    SamplerState samp = SamplerDescriptorHeap[0];
    return tex.Sample(samp, input.uv);
}
