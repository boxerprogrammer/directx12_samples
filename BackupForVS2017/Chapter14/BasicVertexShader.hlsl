#include"Type.hlsli"
SamplerState smp : register(s0);
SamplerState clutSmp : register(s1);
SamplerComparisonState shadowSmp : register(s2);

//マテリアル用スロット
cbuffer MaterialBuffer : register(b0) {
	float4 diffuse;
	float power;
	float3 specular;
	float3 ambient;
};
//マテリアル用
Texture2D<float4> tex : register(t0);//通常テクスチャ
Texture2D<float4> sph : register(t1);//スフィアマップ(乗算)
Texture2D<float4> spa : register(t2);//スフィアマップ(加算)
Texture2D<float4> toon : register(t3);//トゥーンテクスチャ

//シャドウマップ用ライト深度テクスチャ
Texture2D<float> lightDepthTex : register(t4);

//シーン管理用スロット
cbuffer SceneBuffer : register(b1) {
	matrix view;//ビュー
	matrix proj;//プロジェクション
	matrix lightCamera;//ライトビュープロジェ
	matrix shadow;//影行列
	float3 eye;//視点
};

//アクター座標変換用スロット
cbuffer TransBuffer : register(b2) {
	matrix world;
}

//ボーン行列配列
cbuffer BonesBuffer : register(b3) {
	matrix bones[512];
}



PrimitiveType PrimitiveVS(float4 pos:POSITION, float4 normal : NORMAL) {
	PrimitiveType output;
	output.svpos = mul(proj, mul(view, pos));
	output.tpos = mul(lightCamera, pos);
	output.normal = normal;
	return output;
}


//頂点シェーダ(頂点情報から必要なものを次の人へ渡す)
//パイプラインに投げるためにはSV_POSITIONが必要
BasicType BasicVS(float4 pos:POSITION,float4 normal:NORMAL,float2 uv:TEXCOORD,min16uint2 boneno:BONENO,min16uint weight:WEIGHT,uint instNo:SV_InstanceID) {
	//1280,720を直で使って構わない。
	BasicType output;
	float fWeight = float(weight) / 100.0f;
	matrix conBone = bones[boneno.x]*fWeight + 
						bones[boneno.y]*(1.0f - fWeight);

	output.pos = mul(world, 
						mul(conBone,pos)
					);
	output.instNo = (float)instNo;
	output.svpos = mul(proj,mul(view, output.pos));
	output.tpos = mul(lightCamera, output.pos);
//	output.tpos.w = 1;
	output.uv = uv;
	normal.w = 0;
	output.normal = mul(world,mul(conBone,normal));
	return output;
}


//影用頂点座標変換
float4 
ShadowVS(float4 pos:POSITION, float4 normal : NORMAL, float2 uv : TEXCOORD, min16uint2 boneno : BONENO, min16uint weight : WEIGHT) :SV_POSITION{
	float fWeight = float(weight) / 100.0f;
	matrix conBone = bones[boneno.x] * fWeight +
						bones[boneno.y] * (1.0f - fWeight);

	pos = mul(world, mul(conBone, pos));
	return  mul(lightCamera, pos);
}

