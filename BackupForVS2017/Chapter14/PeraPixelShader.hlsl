#include"Type.hlsli"
Texture2D<float4> tex : register(t0);//通常カラー
Texture2D<float4> texNormal : register(t1);//法線
Texture2D<float4> texHighLum : register(t2);//高輝度
Texture2D<float4> texShrinkHighLum : register(t3);//縮小バッファ高輝度
Texture2D<float4> texShrink : register(t4);//縮小バッファ通常

Texture2D<float4> distTex : register(t5);

//深度値検証用
Texture2D<float> depthTex : register(t6);//デプス
Texture2D<float> lightDepthTex : register(t7);//ライトデプス


SamplerState smp : register(s0);
cbuffer Weights : register(b0) {
	//CPUからfloat[8]で渡されたものを
	//正しく受け取ろうとするとfloat4[2]に
	//せざるを得ないため↓のような書き方になる
	float4 wgts[2];
};


float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp,float2 uv,float dx,float dy,float4 rect){
	float4 ret = tex.Sample(smp, uv);

	float l1 = -dx, l2 = -2 * dx;
	float r1 = dx, r2 = 2 * dx;
	float u1 = -dy, u2 = -2 * dy;
	float d1 = dy, d2 = 2 * dy;
	l1 = max(uv.x + l1,rect.x)-uv.x;
	l2 = max(uv.x + l2,rect.x)-uv.x;
	r1 = min(uv.x + r1, rect.z-dx) - uv.x;
	r2 = min(uv.x + r2, rect.z-dx) - uv.x;

	u1 = max(uv.y + u1, rect.y) - uv.y;
	u2 = max(uv.y + u2, rect.y) - uv.y;
	d1 = min(uv.y + d1, rect.w-dy) - uv.y;
	d2 = min(uv.y + d2, rect.w-dy) - uv.y;

	return float4((
		  tex.Sample(smp, uv + float2(l2, u2)).rgb
		+ tex.Sample(smp, uv + float2(l1, u2)).rgb*4
		+ tex.Sample(smp, uv + float2(0, u2)).rgb*6
		+ tex.Sample(smp, uv + float2(r1, u2)).rgb*4
		+ tex.Sample(smp, uv + float2(r2, u2)).rgb

		+ tex.Sample(smp, uv + float2(l2,u1)).rgb*4
		+ tex.Sample(smp, uv + float2(l1,u1)).rgb*16
		+ tex.Sample(smp, uv + float2(0,u1)).rgb*24
		+ tex.Sample(smp, uv + float2(r1,u1)).rgb*16
		+ tex.Sample(smp, uv + float2(r2,u1)).rgb*4

		+ tex.Sample(smp, uv + float2(l2, 0)).rgb*6
		+ tex.Sample(smp, uv + float2(l1, 0)).rgb*24
		+ ret.rgb*36
		+ tex.Sample(smp, uv + float2(r1, 0)).rgb*24
		+ tex.Sample(smp, uv + float2(r2, 0)).rgb*6

		+ tex.Sample(smp, uv + float2(l2, d1)).rgb*4
		+ tex.Sample(smp, uv + float2(l1, d1)).rgb*16
		+ tex.Sample(smp, uv + float2(0, d1)).rgb*24
		+ tex.Sample(smp, uv + float2(r1, d1)).rgb*16
		+ tex.Sample(smp, uv + float2(r2, d1)).rgb*4

		+ tex.Sample(smp, uv + float2(l2, d2)).rgb
		+ tex.Sample(smp, uv + float2(l1, d2)).rgb*4
		+ tex.Sample(smp, uv + float2(0, d2)).rgb*6
		+ tex.Sample(smp, uv + float2(r1, d2)).rgb*4
		+ tex.Sample(smp, uv + float2(r2, d2)).rgb
	) / 256.0f, ret.a);
}

float4 PeraPS(PeraType input) : SV_TARGET{
	if (input.uv.x<0.2&&input.uv.y < 0.2) {//深度出力
		float depth = depthTex.Sample(smp, input.uv*5);
		depth = 1.0f - pow(depth, 30);
		return float4(depth, depth, depth, 1);
	}else if (input.uv.x < 0.2&&input.uv.y < 0.4) {//ライトからの深度出力
		float depth = lightDepthTex.Sample(smp, (input.uv-float2(0,0.2)) * 5);
		depth = 1 - depth;
		return float4(depth, depth, depth, 1);
	}else if (input.uv.x < 0.2&&input.uv.y < 0.6) {//法線出力
		return texNormal.Sample(smp, (input.uv - float2(0, 0.4)) * 5);
	}else if (input.uv.x < 0.2&&input.uv.y < 0.8) {//高輝度縮小
		return texShrinkHighLum.Sample(smp, (input.uv - float2(0, 0.6)) * 5);
	}else if (input.uv.x < 0.2) {//通常縮小
		return texShrink.Sample(smp, (input.uv - float2(0, 0.8)) * 5);
	}

	float w, h, miplevels;
	tex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;

	float2 nmXY=distTex.Sample(smp,input.uv).rg;
	nmXY = (nmXY*2.0f) - 1.0f;

	float4 bloomAccum = float4(0, 0, 0, 0);
	float2 uvSize = float2(1, 0.5);
	float2 uvOfst = float2(0, 0);
	for (int i = 0; i < 8; ++i) {
		bloomAccum += Get5x5GaussianBlur(texShrinkHighLum, smp, input.uv*uvSize+uvOfst,dx,dy,float4(uvOfst,uvOfst+uvSize));
		uvOfst.y += uvSize.y;
		uvSize *= 0.5f;
	}

	//画面真ん中からの深度の差を測る
	float depthDiff=abs(depthTex.Sample(smp, float2(0.5,0.5)) - depthTex.Sample(smp, input.uv));
	depthDiff = pow(depthDiff,0.5f);
	uvSize = float2(1, 0.5);
	uvOfst = float2(0, 0);
	float t = depthDiff*8;
	float no;
	t = modf(t, no);
	float4 retColor[2];
	retColor[0]= tex.Sample(smp, input.uv);//通常テクスチャ
	if (no == 0.0f) {
		retColor[1]= Get5x5GaussianBlur(texShrink, smp, input.uv*uvSize + uvOfst, dx, dy, float4(uvOfst, uvOfst + uvSize));
	}else{
		for (int i = 1; i <= 8; ++i) {
			if (i - no < 0)continue;
			retColor[i-no]= Get5x5GaussianBlur(texShrink, smp, input.uv*uvSize + uvOfst, dx, dy, float4(uvOfst, uvOfst + uvSize));
			uvOfst.y += uvSize.y;
			uvSize *= 0.5f;
			if (i - no > 1) {
				break;
			}
		}
	}
	return lerp(retColor[0],retColor[1],t);
		//+//通常テクスチャ
		//Get5x5GaussianBlur(texHighLum, smp, input.uv, dx, dy,float4(0,0,1,1)) + //1枚目高輝度をぼかし
		//saturate(bloomAccum);//縮小ぼかし済み
	//↓ディファードシェーディング実験用コード↓
	//float4 normal=texNormal.Sample(smp, input.uv);
	//normal = normal * 2.0f - 1.0f;
	//float3 light = normalize(float3(1.0f, -1.0f, 1.0f));
	//const float ambient = 0.25f;
	//float diffB = max(saturate(dot(normal.xyz, -light)),ambient);
	//return tex.Sample(smp, input.uv)*float4(diffB,diffB,diffB,1);


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

//メインテクスチャを5x5ブラーでぼかすピクセルシェーダ
BlurOutput BlurPS(PeraType input)
{
	float w,h,miplevels;
	tex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;
	BlurOutput ret;
	ret.col= tex.Sample(smp, input.uv);//Get5x5GaussianBlur(tex, smp, input.uv, dx, dy, float4(0, 0, 1, 1));
	ret.highLum= Get5x5GaussianBlur(texHighLum, smp, input.uv, dx, dy, float4(0, 0, 1, 1));
	return ret;
}