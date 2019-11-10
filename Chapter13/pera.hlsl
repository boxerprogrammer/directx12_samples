Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

struct Output {
	float4 pos: SV_POSITION;
	float2 uv:TEXCOORD;
};

Output VS(float4 pos:POSITION, float2 uv : TEXCOORD) {
	Output output;
	output.pos = pos;
	output.uv = uv;
	return output;
}
float4 PS(Output input) : SV_TARGET{
	float4 col = tex.Sample(smp,input.uv);
	//return col;
	//float Y = dot(col.rgb, float3(0.299, 0.587, 0.114));
	//return float4(Y, Y, Y, 1);
	//float b = dot(col.rgb, float3(0.2126f, 0.7152f, 0.0722f));
	//return float4(b, b, b, 1);
	float w, h, level;
	tex.GetDimensions(0, w, h, level);

	float dx = 2.0f / w;
	float dy = 2.0f / h;
	float4 ret = float4(0, 0, 0, 0);
	//今のピクセルを中心に縦横5つずつになるよう加算する
//最上段
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 1 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, 2 * dy)) * 4 / 256;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, 2 * dy)) * 6 / 256;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, 2 * dy)) * 4 / 256;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 1 / 256;
	//ひとつ上段
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 1 * dy)) * 4 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, 1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, 1 * dy)) * 24 / 256;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, 1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 1 * dy)) * 4 / 256;
	//中心列
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0 * dy)) * 6 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, 0 * dy)) * 24 / 256;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, 0 * dy)) * 36 / 256;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, 0 * dy)) * 24 / 256;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0 * dy)) * 6 / 256;
	//一つ下段
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -1 * dy)) * 4 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, -1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, -1 * dy)) * 24 / 256;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, -1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -1 * dy)) * 4 / 256;
	//最下段
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 1 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, -2 * dy)) * 4 / 256;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, -2 * dy)) * 6 / 256;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, -2 * dy)) * 4 / 256;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 1 / 256;

	return ret;
}