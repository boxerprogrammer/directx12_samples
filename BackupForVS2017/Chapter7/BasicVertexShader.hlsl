#include"BasicType.hlsli"
Texture2D<float4> tex:register(t0);//0番スロットに設定されたテクスチャ
SamplerState smp:register(s0);//0番スロットに設定されたサンプラ

//定数バッファ
cbuffer cbuff0 : register(b0) {
	matrix world;//ワールド変換行列
	matrix viewproj;//ビュープロジェクション行列
};


BasicType BasicVS(float4 pos : POSITION , float4 normal : NORMAL, float2 uv : TEXCOORD, min16uint2 boneno : BONE_NO, min16uint weight : WEIGHT) {
	BasicType output;//ピクセルシェーダへ渡す値
	output.svpos = mul(mul(viewproj,world),pos);//シェーダでは列優先なので注意
	normal.w = 0;//ここ重要(平行移動成分を無効にする)
	output.normal = mul(world,normal);//法線にもワールド変換を行う
	output.uv = uv;
	return output;
}
