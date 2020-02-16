#include"Type.hlsli"
Texture2D<float4> tex : register(t0);//通常カラー
Texture2D<float4> texNorm : register(t1);//法線
Texture2D<float4> texHighLum : register(t2);//高輝度

float4 GaussianFilteredColor5x5(Texture2D<float4> tex, SamplerState s, float2 uv, float dx, float dy) {
	float4 center = tex.Sample(s, uv);
	float4 ret =
		tex.Sample(s, uv + float2(-dx * 2, -dy * 2)) +
		tex.Sample(s, uv + float2(-dx * 1, -dy * 2)) * 4 +
		tex.Sample(s, uv + float2(-dx * 0, -dy * 2)) * 6 +
		tex.Sample(s, uv + float2(dx * 1, -dy * 2)) * 4 +
		tex.Sample(s, uv + float2(dx * 2, -dy * 2)) +

		tex.Sample(s, uv + float2(-dx * 2, -dy * 1)) * 4 +
		tex.Sample(s, uv + float2(-dx * 1, -dy * 1)) * 16 +
		tex.Sample(s, uv + float2(-dx * 0, -dy * 1)) * 24 +
		tex.Sample(s, uv + float2(dx * 1, -dy * 1)) * 16 +
		tex.Sample(s, uv + float2(dx * 2, -dy * 1)) * 4 +

		tex.Sample(s, uv + float2(-dx * 2, dy * 0)) * 6 +
		tex.Sample(s, uv + float2(-dx * 1, dy * 0)) * 24 +
		center * 36 +
		tex.Sample(s, uv + float2(dx * 1, dy * 0)) * 24 +
		tex.Sample(s, uv + float2(dx * 2, dy * 0)) * 6 +

		tex.Sample(s, uv + float2(-dx * 2, dy * 1)) * 4 +
		tex.Sample(s, uv + float2(-dx * 1, dy * 1)) * 16 +
		tex.Sample(s, uv + float2(-dx * 0, dy * 1)) * 24 +
		tex.Sample(s, uv + float2(dx * 1, dy * 1)) * 16 +
		tex.Sample(s, uv + float2(dx * 2, dy * 1)) * 4 +

		tex.Sample(s, uv + float2(-dx * 2, dy * 2)) +
		tex.Sample(s, uv + float2(-dx * 1, dy * 2)) * 4 +
		tex.Sample(s, uv + float2(-dx * 0, dy * 2)) * 6 +
		tex.Sample(s, uv + float2(dx * 1, dy * 2)) * 4 +
		tex.Sample(s, uv + float2(dx * 2, dy * 2));
	return float4((ret.rgb / 256.0f), ret.a);

}
SamplerState smp : register(s0);

struct ShrinkOutput {
	float4 highLum:SV_TARGET0;
	float4 colForDof:SV_TARGET1;
};

ShrinkOutput ShrinkPS(PeraType input) {
	ShrinkOutput o;
	float w, h, miplevel;
	tex.GetDimensions(0,w, h, miplevel);
	float dx = 1.0f / w;
	float dy = 1.0f / h;
	o.highLum = GaussianFilteredColor5x5(texHighLum,smp, input.uv,dx,dy);
	o.colForDof = GaussianFilteredColor5x5(tex, smp, input.uv, dx, dy);
	return o;//
}