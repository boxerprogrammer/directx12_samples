Texture2D<float4> tex:register(t0);//0番スロットに設定されたテクスチャ
SamplerState smp:register(s0);//0番スロットに設定されたサンプラ

//定数バッファ
cbuffer cbuff0 : register(b0) {
	matrix mat;//変換行列
};

struct Matrix{
	matrix world;
	matrix view;
	matrix proj;
};
ConstantBuffer<Matrix> m: register(b1);

//頂点シェーダ→ピクセルシェーダへのやり取りに使用する
//構造体
struct Output {
	float4 svpos:SV_POSITION;//システム用頂点座標
	float2 uv:TEXCOORD;//UV値
};

Output BasicVS(float4 pos : POSITION,float2 uv:TEXCOORD) {
	Output output;//ピクセルシェーダへ渡す値
	output.svpos = mul(mat,pos);
	output.uv = uv;
	return output;
}

float4 BasicPS(Output input ) : SV_TARGET{
	return float4(tex.Sample(smp,input.uv));
}