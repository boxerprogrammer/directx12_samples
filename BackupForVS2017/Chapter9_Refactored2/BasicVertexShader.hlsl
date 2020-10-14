#include"BasicType.hlsli"
Texture2D<float4> tex:register(t0);//0番スロットに設定されたテクスチャ(ベース)
Texture2D<float4> sph:register(t1);//1番スロットに設定されたテクスチャ(乗算)
Texture2D<float4> spa:register(t2);//2番スロットに設定されたテクスチャ(加算)
Texture2D<float4> toon:register(t3);//3番スロットに設定されたテクスチャ(トゥーン)

SamplerState smp:register(s0);//0番スロットに設定されたサンプラ
SamplerState smpToon:register(s1);//1番スロットに設定されたサンプラ

//定数バッファ0
cbuffer SceneData : register(b0) {
	matrix view;
	matrix proj;//ビュープロジェクション行列
	float3 eye;
};
cbuffer Transform : register(b1) {
	matrix world;//ワールド変換行列
}

//定数バッファ1
//マテリアル用
cbuffer Material : register(b2) {
	float4 diffuse;//ディフューズ色
	float4 specular;//スペキュラ
	float3 ambient;//アンビエント
};


BasicType BasicVS(float4 pos : POSITION , float4 normal : NORMAL, float2 uv : TEXCOORD) {
	BasicType output;//ピクセルシェーダへ渡す値
	pos = mul(world, pos);
	output.svpos = mul(mul(proj,view),pos);//シェーダでは列優先なので注意
	output.pos = mul(view, pos);
	normal.w = 0;//ここ重要(平行移動成分を無効にする)
	output.normal = mul(world,normal);//法線にもワールド変換を行う
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	output.ray = normalize(pos.xyz - mul(view,eye));//視線ベクトル

	return output;
}
