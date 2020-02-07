#pragma once

#include<vector>
#include<array>
#include<string>
#include<DirectXMath.h>
#include<d3d12.h>
#include<wrl.h>
#include<map>
#include<unordered_map>

struct PrimitiveVertex {
	DirectX::XMFLOAT3 pos;//座標
	DirectX::XMFLOAT3 normal;//法線ベクトル
};

using Microsoft::WRL::ComPtr;
//プリミティブ形状オブジェクト基底クラス
class PrimitiveActor
{
public:
	PrimitiveActor();
	virtual ~PrimitiveActor();
	
	virtual void Move(float x, float y, float z)=0;
	virtual void Rotate(float x, float y, float z)=0;

	virtual const DirectX::XMFLOAT3& GetPosition()const=0;
	virtual const DirectX::XMFLOAT3& GetRotate()const=0;

	virtual void Update()=0;
	virtual void Draw(bool isShadow = false)=0;

};

