Texture2D<float4> texNorm : register(t1);//法線
//深度値実験用
Texture2D<float> depthTex : register(t4);//デプス

SamplerState smp : register(s0);

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

//プロジェクションと逆プロジェクションのため
cbuffer sceneBuffer : register(b2) {
	matrix view;//ビュー
	matrix proj;//プロジェクション
	matrix invProj;//逆プロジェクション
	matrix lightCamera;//ライトビュープロジェ
	matrix shadow;//影行列
	float3 eye;//視点
};

float random(float2 uv) {
	return frac(sin(dot(uv, float2(12.9898, 78.233)))*43758.5453);
}
struct Input {
	float4 pos: SV_POSITION;
	float2 uv:TEXCOORD;
};
//スクリーンスペースアンビエントオクルージョン用
float SsaoPS(Input input) :SV_TARGET{
	float w,h,mip;
	depthTex.GetDimensions(0,w, h, mip);

	float dx = 1.0f / w, dy = 1.0f / h;
	float dp = depthTex.Sample(smp, input.uv);
	float4 pos = float4(input.uv,dp,1);
	//①元の座標の復元
	pos.xy = pos.xy*float2(2, -2) + float2(-1, 1);
	pos = mul(invProj, pos);
	pos /= pos.w;//同次座標で割るのを忘れずに

	float ao = 0.0;//当たった時だけ加算される
	float accum = 0.0;//πに当たるやつ(すべて1だった時の総和)
	const int trycnt = tryCount%400;
	const float radius = aoRadius;
	if (dp < 1.0f) {
		float4 norm = texNorm.Sample(smp, input.uv);
		norm.xyz = norm.xyz * 2 - 1;
		for (int i = 0; i < trycnt; ++i) {
			//②ランダムな方向のベクトルωを作る
			float rndX = random(input.uv / 2.0f + float2((float)i*dx, (float)i*dy) / 2.0f);
			float rndY = random(input.uv / 2.0f + float2(rndX, i*dy) / 2.0f);
			float rndZ = random(float2(rndX, rndY));

			//③法線ベクトルを取得し、ランダム方向ベクトルと内積を取り
			//法線の反対側を向いてたら反転する

			float3 omega = normalize(float3(rndX, rndY, rndZ) * 2 - 1);
			float dt = dot(normalize(norm.xyz), omega);//内積=cosθ
			if (dt == 0.0f)continue;
			float sgn = sign(dt);//内積の符号を判別
			omega *= sgn;//法線の反対球に入ってたら法線側の半球に持ってくる
			dt *= sgn;
			//現在の座標にランダムベクトルを加算
			//④この座標がどこかにめり込んでるかどうかを調査する
			float4 rpos = mul(proj, float4(pos.xyz + omega * radius,1));
			rpos /= rpos.w;
			rpos.xy = (rpos.xy + float2(1, -1))*float2(0.5, -0.5);

			ao += step(depthTex.Sample(smp, rpos.xy), rpos.z)*dt;
			accum += dt;
		}
		ao /= accum;
	}
	return 1 - ao;


}