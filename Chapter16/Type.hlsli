
struct BasicType {
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 tpos : TPOS;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
	float instNo : INSTNO;
};



struct PrimitiveType {
	float4 svpos:SV_POSITION;
	float4 tpos : TPOS;
	float4 normal:NORMAL;
};

struct PixelOutput {
	float4 col:SV_TARGET0;//通常のレンダリング結果
	float4 normal:SV_TARGET1;//法線
	float4 highLum:SV_TARGET2;//高輝度(High Luminance)
};

//ペラポリゴン描画用
struct PeraType {
	float4 pos: SV_POSITION;
	float2 uv:TEXCOORD;
};

struct BlurOutput {
	float4 highLum:SV_TARGET0;//高輝度(High Luminance)
	float4 col:SV_TARGET1;//通常のレンダリング結果
};
