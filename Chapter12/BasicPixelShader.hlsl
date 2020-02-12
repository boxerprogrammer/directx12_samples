#include"Type.hlsli"
SamplerState smp : register(s0);
SamplerState clutSmp : register(s1);

//シーン管理用スロット
cbuffer SceneBuffer : register(b1) {
	matrix view;//ビュー
	matrix proj;//プロジェクション
	float3 eye;//視点
};

//マテリアル用スロット
cbuffer MaterialBuffer : register(b0) {
	float4 diffuse;
	float power;
	float3 specular;
	float3 ambient;
};
Texture2D<float4> tex : register(t0);//通常テクスチャ
Texture2D<float4> sph : register(t1);//スフィアマップ(乗算)
Texture2D<float4> spa : register(t2);//スフィアマップ(加算)
Texture2D<float4> toon : register(t3);//トゥーンテクスチャ



//ピクセルシェーダ
float4 BasicPS(BasicType input):SV_TARGET {
	float3 eyeray = normalize(input.pos-eye);
	float3 light = normalize(float3(1,-1,1));
	float3 rlight = reflect(light, input.normal);
		
	//スペキュラ輝度
	float p = saturate(dot(rlight, -eyeray));

	//MSDNのpowのドキュメントによると
	//p=0だったりp==0&&power==0のときNANの可能性が
	//あるため、念のため以下のようなコードにしている
	//https://docs.microsoft.com/ja-jp/windows/win32/direct3dhlsl/dx-graphics-hlsl-pow
	float specB = 0;
	if (p > 0 && power > 0) {
		specB=pow(p, power);
	}

	//ディフューズ明るさ		
	float diffB = dot(-light, input.normal);
	float4 toonCol = toon.Sample(clutSmp, float2(0, 1 - diffB));
	

	float4 texCol =  tex.Sample(smp, input.uv);
	
	//col.rgb= pow(col.rgb, 1.0 / 2.2);
	float2 spUV= (input.normal.xy
		*float2(1, -1) //まず上下だけひっくりかえす
		+ float2(1, 1)//(1,1)を足して-1～1を0～2にする
		) / 2;
	float4 sphCol = sph.Sample(smp, spUV);
	float4 spaCol = spa.Sample(smp, spUV);
	
	//float4 ret= spaCol + sphCol * texCol*toonCol*diffuse + float4(ambient*0.6, 1)
	float4 ret = float4((spaCol + sphCol * texCol * toonCol*diffuse).rgb,diffuse.a)
		+ float4(specular*specB, 1);
	
	//float rim = pow(1 - dot(input.normal, -eyeray),2);
	//return float4(ret.rgb+float3(rim,rim*0.2,rim*0.2),ret.a);
	//return float4(pow(ret.rgb,2.2), ret.a);
	return ret;
}