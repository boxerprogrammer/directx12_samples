Texture2D<float4> tex:register(t0);//0番スロットに設定されたテクスチャ
SamplerState smp:register(s0);//0番スロットに設定されたサンプラ

//定数バッファ0
cbuffer Matrices : register(b0) {
	matrix world;//ワールド変換行列
	matrix viewproj;//ビュープロジェクション行列
};
//定数バッファ1
//マテリアル用
cbuffer Material : register(b1) {
	float4 diffuse;//ディフューズ色
	float4 specular;//スペキュラ
	float3 ambient;//アンビエント
};

//頂点シェーダ→ピクセルシェーダへのやり取りに使用する
//構造体
struct Output {
	float4 svpos:SV_POSITION;//システム用頂点座標
	float4 normal:NORMAL;//法線ベクトル
	float2 uv:TEXCOORD;//UV値
};

Output BasicVS(float4 pos : POSITION , float4 normal : NORMAL, float2 uv : TEXCOORD, min16uint2 boneno : BONE_NO, min16uint weight : WEIGHT) {
	Output output;//ピクセルシェーダへ渡す値
	output.svpos = mul(mul(viewproj,world),pos);//シェーダでは列優先なので注意
	normal.w = 0;//ここ重要(平行移動成分を無効にする)
	output.normal = mul(world,normal);//法線にもワールド変換を行う
	output.uv = uv;
	return output;
}

float4 BasicPS(Output input ) : SV_TARGET{
	float3 light = normalize(float3(1,-1,1));
	float brightness = dot(-light, input.normal);
	return tex.Sample(smp, input.uv)*diffuse;
	return float4(brightness, brightness, brightness, 1)*diffuse;
	//return float4(input.normal.xyz,1);
	//return float4(tex.Sample(smp,input.uv));
}