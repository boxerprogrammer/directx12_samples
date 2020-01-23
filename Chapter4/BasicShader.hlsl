struct Output {
	float4 pos:POSITION;
	float4 svpos:SV_POSITION;
};

Output BasicVS(float4 pos : POSITION) {
	Output output;
	output.pos = pos;
	output.svpos = pos;
	return output;
}

float4 BasicPS(Output input ) : SV_TARGET{
	//return float4(1,1,1,1);
	return float4((float2(0,1)+ input.pos.xy)*0.5f,1,1);
}