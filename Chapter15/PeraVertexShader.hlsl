Texture2D<float4> tex : register(t0);//�ʏ�J���[
Texture2D<float4> texNormal : register(t1);//�@��
Texture2D<float4> texHighLum : register(t2);//���P�x
Texture2D<float4> texShrinkHighLum : register(t3);//�k���o�b�t�@���P�x
Texture2D<float4> texShrink : register(t4);//�k���o�b�t�@�ʏ�



Texture2D<float4> distTex : register(t5);

//�[�x�l�p
Texture2D<float> depthTex : register(t6);//�f�v�X
Texture2D<float> lightDepthTex : register(t7);//���C�g�f�v�X

Texture2D<float> texSSAO : register(t8);//SSAO

SamplerState smp : register(s0);
cbuffer Weights : register(b0) {
	//CPU����float[8]�œn���ꂽ���̂�
	//�������󂯎�낤�Ƃ����float4[2]��
	//������𓾂Ȃ����߁��̂悤�ȏ������ɂȂ�
	float4 wgts[2];
};



struct Output {
	float4 pos: SV_POSITION;
	float2 uv:TEXCOORD;
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

Output PeraVS(float4 pos:POSITION, float2 uv : TEXCOORD) {
	Output output;
	output.pos = pos;
	output.uv = uv;
	return output;
}
struct BlurOutput {
	float4 highLum:SV_TARGET0;//���P�x(High Luminance)
	float4 col:SV_TARGET1;//�ʏ�̃����_�����O����
};