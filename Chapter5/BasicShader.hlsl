float4 BasicVS(float4 pos:POSITION) :SV_POSITION{
	return pos;
}

float4 BasicPS(float4 pos : SV_POSITION) : SV_TARGET{
	return float(1,1,1,1);
}