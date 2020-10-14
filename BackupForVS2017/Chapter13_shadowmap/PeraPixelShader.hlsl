#include"Type.hlsli"


SamplerState smp : register(s0);
Texture2D<float4> tex : register(t0);//通常テクスチャ
Texture2D<float4> distTex : register(t1);//歪みテクスチャ

//深度値検証用
Texture2D<float> depthTex : register(t2);//デプス
Texture2D<float> lightDepthTex : register(t3);//ライトデプス


cbuffer Weights : register(b0) {
	//CPUからfloat[8]で渡されたものを
	//正しく受け取ろうとするとfloat4[2]に
	//せざるを得ないため↓のような書き方になる
	float4 wgts[2];
};


float4 PeraPS(PeraType input) : SV_TARGET{
	//if (input.uv.x<0.2&&input.uv.y < 0.2) {
	//	float depth = depthTex.Sample(smp, input.uv*5);
	//	depth = 1.0f - pow(depth, 30);
	//	return float4(depth, depth, depth, 1);
	//}else if (input.uv.x < 0.2&&input.uv.y < 0.4) {
	//	float depth = lightDepthTex.Sample(smp, (input.uv-float2(0,0.2)) * 5);
	//	//depth = 1.0f - depth;
	//	return float4(depth, depth, depth, 1);
	//}


	float2 nmXY=distTex.Sample(smp,input.uv).rg;
	nmXY = (nmXY*2.0f) - 1.0f;
	return tex.Sample(smp, input.uv);

	float w,h,miplevels;
	tex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;
	float4 col = tex.Sample(smp,input.uv);
	float3 ret = col.rgb * wgts[0];
	for (int i = 1; i < 8; ++i) {
		ret += wgts[i>>2][i%4] * tex.Sample(smp, input.uv + float2(dx*i, 0));
		ret += wgts[i >> 2][i % 4] * tex.Sample(smp, input.uv - float2(dx*i, 0));
	}
	return float4(ret, col.a);
	//return col;
	//ret = float4(1.0-ret.rgb,col.a);
	//float Y = dot(ret.rgb, float3(0.299f, 0.587f, 0.114f));
	//Y = step(0.1,pow(Y, 25));

	//return float4(Y,Y,Y,col.a);
	//return float4(1.0f-col.rgb,col.a);

	//
	float Y = dot(col.rgb, float3(0.299f, 0.587f, 0.114f));
	
	Y -= fmod(Y, 0.25);
	return float4(Y*0.7, Y, Y*0.7, 1);

	return float4(float3(1,1,1)-col.rgb,col.a);
}

float4 VerticalBlurPS(PeraType input) : SV_TARGET{
		float w,h,miplevels;
	tex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;
	float4 col = tex.Sample(smp,input.uv);
	float3 ret = col.rgb * wgts[0];
	for (int i = 1; i < 8; ++i) {
		ret += wgts[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0, dy*i));
		ret += wgts[i >> 2][i % 4] * tex.Sample(smp, input.uv - float2(0, dy*i));
	}
	return float4(ret,col.a);
}