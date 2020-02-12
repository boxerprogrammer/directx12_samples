#include"Type.hlsli"

PeraType PeraVS(float4 pos:POSITION, float2 uv : TEXCOORD) {
	PeraType output;
	output.pos = pos;
	output.uv = uv;
	return output;
}
