#include"Type.hlsli"
cbuffer Weight : register(b0) {
	float4 bkweights[2];
};
Texture2D<float4> tex : register(t0);
Texture2D<float4> distTex : register(t1);

SamplerState smp : register(s0);

PeraType PeraVS(float4 pos:POSITION, float2 uv : TEXCOORD) {
	PeraType output;
	output.pos = pos;
	output.uv = uv;
	return output;
}
