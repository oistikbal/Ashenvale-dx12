#define RS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | SAMPLER_HEAP_DIRECTLY_INDEXED | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED ), " \
            "RootConstants(num32BitConstants=17, b0)"

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD2;
};

struct SceneBuffer
{
    float4x4 vp;
    uint instanceBufferIdx;
};

struct InstanceData
{
    float4x4 worldMatrix;
};

ConstantBuffer<SceneBuffer> sb : register(b0);

[RootSignature(RS)]
VSOutput vs_main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
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
    
    StructuredBuffer<InstanceData> instances = ResourceDescriptorHeap[sb.instanceBufferIdx];
    InstanceData data = instances[instanceId];
    
    VSOutput output;
    float4 worldPos = mul(data.worldMatrix, float4(positions[vertexId], 0.0f, 1.0f));
    output.position = mul(sb.vp, worldPos);
    output.uv = uv[vertexId];
    return output;
}

float4 ps_main(VSOutput input) : SV_Target
{

    
    Texture2D<float4> tex = ResourceDescriptorHeap[0];
    SamplerState samp = SamplerDescriptorHeap[0];
    return tex.Sample(samp, input.uv);
}
