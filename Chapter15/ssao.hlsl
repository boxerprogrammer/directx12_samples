//SSAO処理のためだけのシェーダ
Texture2D<float4> normtex:register(t1);//1パス目の法線描画
Texture2D<float> depthtex:register(t6);//1パス目の深度テクスチャ

SamplerState smp:register(s0);

//返すのはSV_POSITIONだけではない
struct Output {
	float4 pos: SV_POSITION;
	float2 uv:TEXCOORD;
};

//元座標復元に必要
cbuffer sceneBuffer : register(b1) {
	matrix view;//ビュー
	matrix proj;//プロジェクション
	matrix invproj;//逆プロジェクション
	matrix lightCamera;//ライトビュープロジェ
	matrix shadow;//影行列
	float3 eye;//視点
};

//現在のUV値を元に乱数を返す
float random(float2 uv) {
	return frac(sin(dot(uv, float2(12.9898f, 78.233f)))*43758.5453f);
}
//SSAO(乗算用の明度のみ情報を返せればよい)
float SsaoPs(Output input) : SV_Target
{
	//matrix<float,2,3> mat;
	//float2x3 mat2;
	float4x4 mat;
	mat._11_12_13_14 = float4(1.0f, 2.0f, 3.0f, 4.0f);

	float dp = depthtex.Sample(smp, input.uv);//現在のUVの深度

	float w, h, miplevels;
	depthtex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0f / w;
	float dy = 1.0f / h;

	//SSAO
	//元の座標を復元する
	float4 respos = mul(invproj, float4(input.uv*float2(2, -2) + float2(-1, 1), dp, 1));
	respos.xyz = respos.xyz / respos.w;
	float div = 0.0f;
	float ao = 0.0f;
	float3 norm = normalize((normtex.Sample(smp, input.uv).xyz * 2) - 1);
	const int trycnt = 256;
	const float radius = 0.5f;
	if (dp < 1.0f) {
		for (int i = 0; i < trycnt; ++i) {
			float rnd1 = random(float2(i*dx, i*dy)) * 2 - 1;
			float rnd2 = random(float2(rnd1, i*dy)) * 2 - 1;
			float rnd3 = random(float2(rnd2, rnd1)) * 2 - 1;
			float3 omega = normalize(float3(rnd1,rnd2,rnd3));
			omega = normalize(omega);
			//乱数の結果法線の反対側に向いてたら反転する
			float dt = dot(norm, omega);
			float sgn = sign(dt);
			omega *= sign(dt);
			//結果の座標を再び射影変換する
			float4 rpos = mul(proj, float4(respos.xyz + omega * radius, 1));
			rpos.xyz /= rpos.w;
			dt *= sgn;
			div += dt;
			//計算結果が現在の場所の深度より奥に入ってるなら遮蔽されているという事なので加算
			ao += step(depthtex.Sample(smp, (rpos.xy + float2(1, -1))*float2(0.5f, -0.5f)), rpos.z)*dt;
		}
		ao /= div;
	}
	return 1.0f - ao;
}


//SSAO
//float SsaoPs(Output input) : SV_Target
//{
//	float dp = depthtex.Sample(smp, input.uv);//現在のUVの深度
//
//	SSAO
//	元の座標を復元する
//	float4 respos = mul(invproj, float4(input.uv*float2(2, -2) + float2(-1, 1), dp, 1));
//	respos.xyz = respos.xyz / respos.w;
//	
//	float oc = 0.0f;
//	float3 norm = normalize((normtex.Sample(smp, input.uv).xyz * 2) - 1);
//	
//	if (dp < 1.0f) {
//		for (int i = 0; i < 256; ++i) {
//			float3 omega = (float3(random(input.uv + float2(i / 100.0f, i / 50.0f)) * 2 - 1,
//				random(input.uv + float2(0.3 + i / 512.0f + 0 / 36000.0f, 0.2 + i / 64.0f)) * 2 - 1,
//				random(input.uv + float2(0.2 + i / 32.0f + 0 / 36000.0f, i / 512.0f)) * 2 - 1));
//			omega = normalize(omega);
//			float dt = dot(norm, omega);
//			float sgn = sign(dt);
//			omega *= sign(dt);
//			float4 rpos = mul(proj, mul(view,float4(respos.xyz + omega, 1)));
//			rpos.xyz /= rpos.w;
//			oc += step(depthtex.Sample(smp, (rpos.xy + float2(1, -1))*float2(0.5, -0.5)), rpos.z)*dot(norm, omega);
//		}
//		oc /= 256.0f;
//	}
//	return 1.0f - oc;
//
//}