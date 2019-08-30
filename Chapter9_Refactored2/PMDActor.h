#pragma once

#include<d3d12.h>
#include<vector>
#include<wrl.h>

class PMDActor
{
private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	ComPtr<ID3D12Resource> _vb=nullptr;
	ComPtr<ID3D12Resource> _ib=nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbv = {};
	D3D12_INDEX_BUFFER_VIEW _ibv = {};
	ComPtr< ID3D12DescriptorHeap> _materialHeap = nullptr;//マテリアルヒープ(5個ぶん)
	bool LoadPMD(const char* filepath);

public:
	PMDActor(const char* filepath);
	~PMDActor();
	PMDActor* Clone();
	void Update();
	void Draw();
};

