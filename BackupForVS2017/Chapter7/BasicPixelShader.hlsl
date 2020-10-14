#include"BasicType.hlsli"
Texture2D<float4> tex:register(t0);//0�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`��
SamplerState smp:register(s0);//0�ԃX���b�g�ɐݒ肳�ꂽ�T���v��

//�萔�o�b�t�@
cbuffer cbuff0 : register(b0) {
	matrix world;//���[���h�ϊ��s��
	matrix viewproj;//�r���[�v���W�F�N�V�����s��
};

float4 BasicPS(BasicType input ) : SV_TARGET{
	float3 light = normalize(float3(1,-1,1));
	float brightness = dot(-light, input.normal);
	return float4(brightness, brightness, brightness, 1);
	//return float4(input.normal.xyz,1);
	//return float4(tex.Sample(smp,input.uv));
}