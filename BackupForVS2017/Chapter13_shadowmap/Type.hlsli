//モデル描画用
struct BasicType {
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 tpos : TPOS;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
	float instNo : INSTNO;
};

//基本形状用
struct PrimitiveType {
	float4 svpos:SV_POSITION;
	float4 tpos : TPOS;
	float4 normal:NORMAL;
};


//ペラポリゴン描画用
struct PeraType {
	float4 pos: SV_POSITION;
	float2 uv:TEXCOORD;
};