#include"BasicType.hlsli"

Texture2D<float4> tex:register(t0);//0番スロットに設定されたテクスチャ(ベース)
Texture2D<float4> sph:register(t1);//1番スロットに設定されたテクスチャ(乗算)
Texture2D<float4> spa:register(t2);//2番スロットに設定されたテクスチャ(加算)
Texture2D<float4> toon:register(t3);//3番スロットに設定されたテクスチャ(トゥーン)

SamplerState smp:register(s0);//0番スロットに設定されたサンプラ
SamplerState smpToon:register(s1);//1番スロットに設定されたサンプラ


//定数バッファ1
//マテリアル用
cbuffer Material : register(b2) {
	float4 diffuse;//ディフューズ色
	float4 specular;//スペキュラ
	float3 ambient;//アンビエント
};




float4 BasicPS(BasicType input ) : SV_TARGET{
	float3 light = normalize(float3(1,-1,1));//光の向かうベクトル(平行光線)
	float3 lightColor = float3(1,1,1);//ライトのカラー(1,1,1で真っ白)

	//ディフューズ計算
	float diffuseB = saturate(dot(-light, input.normal));
	float4 toonDif =  toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	//光の反射ベクトル
	float3 refLight= normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)),specular.a);
	
	//スフィアマップ用UV
	float2 sphereMapUV = input.vnormal.xy;
	sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);

	float4 texColor = tex.Sample(smp, input.uv); //テクスチャカラー

	return saturate(toonDif//輝度(トゥーン)
		* diffuse//ディフューズ色
		*texColor//テクスチャカラー
		*sph.Sample(smp, sphereMapUV))//スフィアマップ(乗算)
		+ saturate(spa.Sample(smp, sphereMapUV)*texColor//スフィアマップ(加算)
		+ float4(specularB *specular.rgb, 1))//スペキュラー
		+ float4(texColor*ambient*0.5,1);//アンビエント(明るくなりすぎるので0.5にしてます)
}