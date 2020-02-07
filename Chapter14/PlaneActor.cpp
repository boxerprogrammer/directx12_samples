#include "PlaneActor.h"
#include<cassert>
#include<DirectXMath.h>

using namespace DirectX;
using namespace std;

PlaneActor::PlaneActor():PlaneActor(10.0f,10.0f)
{
}

PlaneActor::PlaneActor(float width, float height) {

}

PlaneActor::PlaneActor(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& normal) {

}
PlaneActor::PlaneActor(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& normal, float width, float height) {
	assert(!(normal.x == 0.0f&&normal.y == 0.0f&&normal.z == 0.0f));
	

}

PlaneActor::~PlaneActor()
{
}
