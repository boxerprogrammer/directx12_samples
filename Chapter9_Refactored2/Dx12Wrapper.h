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
	//テクスチャローダテーブルの作成
	void CreateTextureLoaderTable();
	//テクスチャ名からテクスチャバッファ作成、中身をコピー
	HRESULT CreateTextureFromFile(const char* texpath);
public:
	Dx12Wrapper();
	~Dx12Wrapper();
	///テクスチャパスから必要なテクスチャバッファへのポインタを返す
	///@param texpath テクスチャファイルパス
	ComPtr<ID3D12Resource> GetTextureByName(const char* texpath);
	
};

