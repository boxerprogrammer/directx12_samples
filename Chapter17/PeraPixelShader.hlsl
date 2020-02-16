#define FXAA_GRAY_AS_LUMA 1
#define FXAA_PC 1
#define FXAA_HLSL_5 1
#include"FXAA.hlsli"
#include"Type.hlsli"


Texture2D<float4> tex : register(t0);//通常カラー
Texture2D<float4> texNorm : register(t1);//法線
Texture2D<float4> texHighLum : register(t2);//高輝度

Texture2D<float4> distTex : register(t3);

//深度値実験用
Texture2D<float> depthTex : register(t4);//デプス
Texture2D<float> lightDepthTex : register(t5);//ライトデプス

Texture2D<float4> bloomTex : register(t6);//ブルーム用縮小バッファ
Texture2D<float4> dofTex : register(t7);//DOF縮小バッファ

Texture2D<float> ssaoTex : register(t8);//SSAOテクスチャ

SamplerState smp : register(s0);
cbuffer Weights : register(b0) {
	//CPUからfloat[8]で渡されたものを
	//正しく受け取ろうとするとfloat4[2]に
	//せざるを得ないため↓のような書き方になる
	float4 wgts[2];
};

cbuffer PostSetting : register(b1) {
	int outline;
	int rimFlg;
	float rimStrength;
	int debugDispFlg;
	int normOutline;
	int directionalLightFlg;
	int aaFlg;
	int bloomFlg;
	int dofFlg;
	int aoFlg;
	int tryCount;
	float aoRadius;
	float4 bloomColor;
	float2 focusPos;
};


float4 GaussianFilteredColor5x5(Texture2D<float4> tex, SamplerState s, float2 uv, float dx, float dy) {
	float4 center = tex.Sample(s, uv);
	float4 ret =
		tex.Sample(s, uv + float2(-dx * 2, -dy * 2)) +
		tex.Sample(s, uv + float2(-dx * 1, -dy * 2)) * 4 +
		tex.Sample(s, uv + float2(-dx * 0, -dy * 2)) * 6 +
		tex.Sample(s, uv + float2(dx * 1, -dy * 2)) * 4 +
		tex.Sample(s, uv + float2(dx * 2, -dy * 2)) +

		tex.Sample(s, uv + float2(-dx * 2, -dy * 1)) * 4 +
		tex.Sample(s, uv + float2(-dx * 1, -dy * 1)) * 16 +
		tex.Sample(s, uv + float2(-dx * 0, -dy * 1)) * 24 +
		tex.Sample(s, uv + float2(dx * 1, -dy * 1)) * 16 +
		tex.Sample(s, uv + float2(dx * 2, -dy * 1)) * 4 +

		tex.Sample(s, uv + float2(-dx * 2, dy * 0)) * 6 +
		tex.Sample(s, uv + float2(-dx * 1, dy * 0)) * 24 +
		center * 36 +
		tex.Sample(s, uv + float2(dx * 1, dy * 0)) * 24 +
		tex.Sample(s, uv + float2(dx * 2, dy * 0)) * 6 +

		tex.Sample(s, uv + float2(-dx * 2, dy * 1)) * 4 +
		tex.Sample(s, uv + float2(-dx * 1, dy * 1)) * 16 +
		tex.Sample(s, uv + float2(-dx * 0, dy * 1)) * 24 +
		tex.Sample(s, uv + float2(dx * 1, dy * 1)) * 16 +
		tex.Sample(s, uv + float2(dx * 2, dy * 1)) * 4 +

		tex.Sample(s, uv + float2(-dx * 2, dy * 2)) +
		tex.Sample(s, uv + float2(-dx * 1, dy * 2)) * 4 +
		tex.Sample(s, uv + float2(-dx * 0, dy * 2)) * 6 +
		tex.Sample(s, uv + float2(dx * 1, dy * 2)) * 4 +
		tex.Sample(s, uv + float2(dx * 2, dy * 2));
	return float4((ret.rgb / 256.0f), ret.a);

}

float4 ShrinkPS(PeraType input) : SV_TARGET{
	return texHighLum.Sample(smp,input.uv);
}



float4 PeraPS(PeraType input) : SV_TARGET{
	if (debugDispFlg) {
		if (input.uv.x < 0.2&&input.uv.y < 0.2) {
			float depth = depthTex.Sample(smp, input.uv * 5);
			depth = 1.0f - pow(depth, 30);
			return float4(depth, depth, depth, 1);
		}
		else if (input.uv.x < 0.2&&input.uv.y < 0.4) {
		   float depth = lightDepthTex.Sample(smp, (input.uv - float2(0,0.2)) * 5);
		   return float4(depth, depth, depth, 1);
		}
		else if (input.uv.x < 0.2&&input.uv.y < 0.6) {
		   return texNorm.Sample(smp, (input.uv - float2(0, 0.4)) * 5);
		}
		else if (input.uv.x < 0.2&&input.uv.y < 0.8) {
		   return texHighLum.Sample(smp, (input.uv - float2(0, 0.6)) * 5);
		}
		else if (input.uv.x < 0.2&&input.uv.y < 1.0) {
		   return bloomTex.Sample(smp, (input.uv - float2(0, 0.8)) * 5);
		}
		else if (input.uv.x < 0.4&&input.uv.y < 0.2) {
		   return dofTex.Sample(smp, (input.uv - float2(0.2, 0.0)) * 5);
		}
		else if (input.uv.x < 0.4&&input.uv.y < 0.4) {
		   float ssao = ssaoTex.Sample(smp, (input.uv - float2(0.2, 0.2)) * 5);
		   return float4(ssao,ssao,ssao,1);
		}
	}

	float w,h,miplevels;
	tex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;


	float edge = depthTex.Sample(smp, input.uv) * 4 -
		depthTex.Sample(smp, input.uv - float2(dx, 0)) -
		depthTex.Sample(smp, input.uv - float2(-dx, 0)) -
		depthTex.Sample(smp, input.uv - float2(0, dy)) -
		depthTex.Sample(smp, input.uv - float2(0, -dy));

	edge = outline ? 1.0f - step(0.0025, edge) : 1.0f;

	float4 baseNorm = texNorm.Sample(smp, input.uv);
	if (baseNorm.a > 0) {
		baseNorm.xyz = baseNorm.xyz * 2 - 1;
		float normEdge =
			step(0.2, dot(baseNorm.xyz, texNorm.Sample(smp, input.uv + float2(dx, 0)).xyz * 2 - 1))*
			step(0.2, dot(baseNorm.xyz, texNorm.Sample(smp, input.uv + float2(0, dy)).xyz * 2 - 1));
		edge = normOutline ? normEdge * edge : edge;
	}
	float2 nmXY = distTex.Sample(smp, input.uv).rg;
	nmXY = (nmXY*2.0f) - 1.0f;
	float4 retcol = tex.Sample(smp, input.uv);
	if (aoFlg) {
		retcol.rgb *= ssaoTex.Sample(smp, input.uv);
	}

	if (aaFlg) {//アンチエイリアシングフラグが立っていればFXAAを有効にする
		FxaaTex InputFXAATex = { smp, tex };
		float3 aa = FxaaPixelShader(
			input.uv,							// FxaaFloat2 pos,
			FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f),		// FxaaFloat4 fxaaConsolePosPos,
			InputFXAATex,							// FxaaTex tex,
			InputFXAATex,							// FxaaTex fxaaConsole360TexExpBiasNegOne,
			InputFXAATex,							// FxaaTex fxaaConsole360TexExpBiasNegTwo,
			float2(dx, dy),							// FxaaFloat2 fxaaQualityRcpFrame,
			FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f),		// FxaaFloat4 fxaaConsoleRcpFrameOpt,
			FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f),		// FxaaFloat4 fxaaConsoleRcpFrameOpt2,
			FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f),		// FxaaFloat4 fxaaConsole360RcpFrameOpt2,
			0.75f,									// FxaaFloat fxaaQualitySubpix,
			0.166f,									// FxaaFloat fxaaQualityEdgeThreshold,
			0.0833f,								// FxaaFloat fxaaQualityEdgeThresholdMin,
			0.0f,									// FxaaFloat fxaaConsoleEdgeSharpness,
			0.0f,									// FxaaFloat fxaaConsoleEdgeThreshold,
			0.0f,									// FxaaFloat fxaaConsoleEdgeThresholdMin,
			FxaaFloat4(0.0f, 0.0f, 0.0f, 0.0f)		// FxaaFloat fxaaConsole360ConstDir,
		).rgb;
		retcol.rgb = aa;
	}

	if (directionalLightFlg) {
		retcol.rgb = retcol.rgb* max(saturate(dot(baseNorm.xyz * 2 - 1, normalize(float3(-1, 1, -1)))),0.25);
	}

	float4 accumBloom = 0;
	float2 dsize = float2(1.0f,0.5f);
	float ofsty = 0.0f;
	if (bloomFlg) {//ブルームフラグが立っていればブルーム合成を有効にする
		accumBloom = GaussianFilteredColor5x5(texHighLum, smp, input.uv, dx, dy);
		for (int i = 0; i < 8; ++i) {
			accumBloom += GaussianFilteredColor5x5(bloomTex, smp, input.uv*dsize + float2(0, ofsty), dx, dy);
			ofsty += dsize.y;
			dsize /= 2.0f;
		}
		accumBloom *= bloomColor;
	}


	if (dofFlg) {//被写界深度フラグが立っていれば被写界深度用処理を有効にする
		if (depthTex.Sample(smp, input.uv) < 1.0) {
			//基準になるdepth
			float baseDepth = depthTex.Sample(smp, focusPos);
			float t = pow(distance(baseDepth, depthTex.Sample(smp, input.uv)), 0.5f);// *8.0f;


			t *= 8.0f;
			float alpha, no;
			alpha = modf(t, no);

			float3 colA, colB;
			if (no == 0) {
				colA = retcol.rgb;
				colB = GaussianFilteredColor5x5(dofTex, smp, input.uv*float2(1.0f, 0.5f), dx, dy);
			}
			else {
				colA = GaussianFilteredColor5x5(dofTex, smp, input.uv*float2(1.0f, 0.5f)*pow(0.5, no - 1) + float2(0, 1.0 - pow(0.5, no - 1)), dx, dy);
				colB = GaussianFilteredColor5x5(dofTex, smp, input.uv*float2(1.0f, 0.5f)*pow(0.5, no) + float2(0, 1 - pow(0.5, no)), dx, dy);
			}
			retcol.rgb = lerp(colA, colB, alpha);
		}
	}
	return float4(retcol.rgb*edge +
		accumBloom.rgb
		, retcol.a);


	float4 col = tex.Sample(smp,input.uv);
	float3 ret = col.rgb * wgts[0];
	for (int i = 1; i < 8; ++i) {
		ret += wgts[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(dx*i, 0));
		ret += wgts[i >> 2][i % 4] * tex.Sample(smp, input.uv - float2(dx*i, 0));
	}
	return float4(ret, col.a);

	//
	float Y = dot(col.rgb, float3(0.299f, 0.587f, 0.114f));

	Y -= fmod(Y, 0.25);
	return float4(Y*0.7, Y, Y*0.7, 1);

	return float4(float3(1,1,1) - col.rgb,col.a);
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