#pragma once
#include<d3d12.h>
#include<map>
#include<d3dcompiler.h>
#include<wrl.h>
#include<string>
class Dx12Wrapper
{
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	std::map<std::string,ComPtr<ID3D12Resource>> _textureTable;
public:
	Dx12Wrapper();
	~Dx12Wrapper();
};

