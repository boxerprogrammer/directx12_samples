SamplerState smp : register(s0);
SamplerState clutSmp : register(s1);
SamplerComparisonState shadowSmp : register(s2);

//マテリアル用スロット
cbuffer materialBuffer : register(b0) {
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
cbuffer sceneBuffer : register(b1) {
	matrix view;//ビュー
	matrix proj;//プロジェクション
	matrix invproj;//プロジェクション
	matrix lightCamera;//ライトビュープロジェ
	matrix shadow;//影行列
	float3 eye;//視点
	
};

//アクター座標変換用スロット
cbuffer transBuffer : register(b2) {
	matrix world;
}

//ボーン行列配列
cbuffer transBuffer : register(b3) {
	matrix bones[512];
}


//返すのはSV_POSITIONだけではない
struct Output {
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 tpos : TPOS;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
	float instNo : INSTNO;
};

struct PixelOutput {
	float4 col:SV_TARGET0;//通常のレンダリング結果
	float4 normal:SV_TARGET1;//法線
	float4 highLum:SV_TARGET2;//高輝度(High Luminance)
};

struct PrimitiveOutput {
	float4 svpos:SV_POSITION;
	float4 tpos : TPOS;
	float4 normal:NORMAL;
};

PrimitiveOutput PrimitiveVS(float4 pos:POSITION, float4 normal : NORMAL) {
	PrimitiveOutput output;
	output.svpos = mul(proj, mul(view, pos));
	output.tpos = mul(lightCamera, pos);
	output.normal = normal;
	return output;
}
//頂点シェーダ(頂点情報から必要なものを次の人へ渡す)
//パイプラインに投げるためにはSV_POSITIONが必要
Output BasicVS(float4 pos:POSITION,float4 normal:NORMAL,float2 uv:TEXCOORD,min16uint2 boneno:BONENO,min16uint weight:WEIGHT,uint instNo:SV_InstanceID) {
	//1280,720を直で使って構わない。
	Output output;
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


