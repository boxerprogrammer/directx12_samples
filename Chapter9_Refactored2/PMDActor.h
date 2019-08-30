#pragma once

#include<d3d12.h>
#include<vector>
#include<wrl.h>

class PMDActor
{
private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	ComPtr< ID3D12DescriptorHeap> _materialHeap = nullptr;//マテリアルヒープ(5個ぶん)
public:
	PMDActor(const char* filepath);
	~PMDActor();
	PMDActor* Clone();
	void Update();
	void Draw();
};

