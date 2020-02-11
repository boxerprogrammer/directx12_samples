#include"BasicType.hlsli"
Texture2D<float4> tex:register(t0);//0番スロットに設定されたテクスチャ
SamplerState smp:register(s0);//0番スロットに設定されたサンプラ

//定数バッファ
cbuffer cbuff0 : register(b0) {
	matrix world;//ワールド変換行列
	matrix viewproj;//ビュープロジェクション行列
};

float4 BasicPS(BasicType input ) : SV_TARGET{
	float3 light = normalize(float3(1,-1,1));
	float brightness = dot(-light, input.normal);
	return float4(brightness, brightness, brightness, 1);
	//return float4(input.normal.xyz,1);
	//return float4(tex.Sample(smp,input.uv));
}