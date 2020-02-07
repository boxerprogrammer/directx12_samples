SamplerState smp : register(s0);
SamplerState clutSmp : register(s1);

//マテリアル用スロット
cbuffer materialBuffer : register(b0) {
	float4 diffuse;
	float power;
	float3 specular;
	float3 ambient;
};
Texture2D<float4> tex : register(t0);//通常テクスチャ
Texture2D<float4> sph : register(t1);//スフィアマップ(乗算)
Texture2D<float4> spa : register(t2);//スフィアマップ(加算)
Texture2D<float4> toon : register(t3);//トゥーンテクスチャ

//シーン管理用スロット
cbuffer sceneBuffer : register(b1) {
	matrix view;//ビュー
	matrix proj;//プロジェクション
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
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
	float weight : WEIGHT;
	float boneno : BONENO;
};

//頂点シェーダ(頂点情報から必要なものを次の人へ渡す)
//パイプラインに投げるためにはSV_POSITIONが必要
Output VS(float4 pos:POSITION,float4 normal:NORMAL,float2 uv:TEXCOORD,min16uint2 boneno:BONENO,min16uint weight:WEIGHT) {
	//1280,720を直で使って構わない。
	Output output;
	float fWeight = float(weight) / 100.0f;
	matrix conBone = bones[boneno.x]*fWeight + 
						bones[boneno.y]*(1.0f - fWeight);

	output.pos = mul(world, 
						mul(conBone,pos)
					);
	output.svpos = mul(proj,mul(view, output.pos));
	output.uv = uv;
	normal.w = 0;
	output.normal = mul(world,normal);
	output.weight = (float)weight/100.0f;
	output.boneno = boneno[0]/122.0;
	//output.uv = uv;
	return output;
}


//ピクセルシェーダ
float4 PS(Output input):SV_TARGET {
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