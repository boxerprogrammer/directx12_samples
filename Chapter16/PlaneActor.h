#pragma once
#include "PrimitiveActor.h"

///平面オブジェクト
class PlaneActor :
	public PrimitiveActor
{
private:
	DirectX::XMFLOAT3 _centerPos;
	DirectX::XMFLOAT3 _normal;
	std::array<PrimitiveVertex, 4> _vertices;
public:
	PlaneActor();
	PlaneActor(float width , float height);
	PlaneActor(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& normal);
	PlaneActor(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& normal,float width , float height);
	~PlaneActor();

	virtual void Move(float x, float y, float z) = 0;
	virtual void Rotate(float x, float y, float z) = 0;

	virtual const DirectX::XMFLOAT3& GetPosition()const = 0;
	virtual const DirectX::XMFLOAT3& GetRotate()const = 0;

	virtual void Update() = 0;
	virtual void Draw(bool isShadow = false) = 0;

};

