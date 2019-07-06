//頂点シェーダ→ピクセルシェーダへのやり取りに使用する
//構造体
struct Output {
	float4 svpos:SV_POSITION;//システム用頂点座標
	float2 uv:TEXCOORD;//UV値
};

Output BasicVS(float4 pos : POSITION,float2 uv:TEXCOORD) {
	Output output;//ピクセルシェーダへ渡す値
	output.svpos = pos;
	output.uv = uv;
	return output;
}

float4 BasicPS(Output input ) : SV_TARGET{
	return float4(input.uv,1,1);
}