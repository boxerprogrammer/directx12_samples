Texture2D<float4> tex:register(t0);//0番スロットに設定されたテクスチャ
Texture2D<float4> sph:register(t1);//1番スロットに設定されたテクスチャ
Texture2D<float4> spa:register(t2);//2番スロットに設定されたテクスチャ
SamplerState smp:register(s0);//0番スロットに設定されたサンプラ

//定数バッファ0
cbuffer Matrices : register(b0) {
	matrix world;//ワールド変換行列
	matrix viewproj;//ビュープロジェクション行列
	float3 eye;
};
//定数バッファ1
//マテリアル用
cbuffer Material : register(b1) {
	float4 diffuse;//ディフューズ色
	float4 specular;//スペキュラ
	float3 ambient;//アンビエント
};

//頂点シェーダ→ピクセルシェーダへのやり取りに使用する
//構造体
struct Output {
	float4 svpos:SV_POSITION;//システム用頂点座標
	float4 normal:NORMAL;//法線ベクトル
	float2 uv:TEXCOORD;//UV値
	float3 ray:VECTOR;//ベクトル
};

Output BasicVS(float4 pos : POSITION , float4 normal : NORMAL, float2 uv : TEXCOORD) {
	Output output;//ピクセルシェーダへ渡す値
	output.svpos = mul(mul(viewproj,world),pos);//シェーダでは列優先なので注意
	normal.w = 0;//ここ重要(平行移動成分を無効にする)
	output.normal = mul(world,normal);//法線にもワールド変換を行う
	output.uv = uv;
	output.ray = normalize(pos.xyz - eye);//視線ベクトル
	return output;
}

float4 BasicPS(Output input ) : SV_TARGET{
	float3 light = normalize(float3(1,-1,1));
	float3 lightColor = float3(1, 1, 1);
	float diffuseB = dot(-light, input.normal);

	float3 up = float3(0, 1, 0);
	float3 right = normalize(cross(up, input.ray));//右ベクトル

	//光の反射ベクトル
	float3 refLight= normalize(reflect(light, input.normal.xyz));
	float specularB = saturate(dot(refLight, -input.ray));

	//視線の反射ベクトル
	float3 refRay = normalize(reflect(input.ray, input.normal.xyz));
	
	float2 sphereMapUV = float2(dot(input.normal.xyz, right),dot(input.normal.xyz, up));
	sphereMapUV=(sphereMapUV + float2(1, -1))*float2(0.5, -0.5);

	float4 color = tex.Sample(smp, input.uv); //テクスチャカラー
	return
		saturate(float4(lightColor * diffuseB, 1)
			*diffuse//ディフューズ色
			*color//テクスチャカラー
			*sph.Sample(smp, sphereMapUV))//スフィアマップ(乗算)

		+ saturate(float4(pow(specularB, specular.w)
			*specular.rgb
			*lightColor, 1))


		+ spa.Sample(smp, sphereMapUV);//スフィアマップ(加算)
		+float4(color*ambient,1);//アンビエント

}