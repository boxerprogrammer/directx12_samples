#pragma once

#include<d3d12.h>
#include<DirectXMath.h>
#include<vector>
#include<wrl.h>

class PMDActor
{
private:

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	//シェーダ側に投げられるマテリアルデータ
	struct MaterialForHlsl {
		DirectX::XMFLOAT3 diffuse; //ディフューズ色
		float alpha; // ディフューズα
		DirectX::XMFLOAT3 specular; //スペキュラ色
		float specularity;//スペキュラの強さ(乗算値)
		DirectX::XMFLOAT3 ambient; //アンビエント色
	};
	//それ以外のマテリアルデータ
	struct AdditionalMaterial {
		std::string texPath;//テクスチャファイルパス
		int toonIdx; //トゥーン番号
		bool edgeFlg;//マテリアル毎の輪郭線フラグ
	};
	//まとめたもの
	struct Material {
		unsigned int indicesNum;//インデックス数
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};


	std::vector<Material> _materials;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;
	
	ComPtr<ID3D12Resource> _vb=nullptr;
	ComPtr<ID3D12Resource> _ib=nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbv = {};
	D3D12_INDEX_BUFFER_VIEW _ibv = {};
	ComPtr< ID3D12DescriptorHeap> _materialHeap = nullptr;//マテリアルヒープ(5個ぶん)



	//マテリアル＆テクスチャのビューを作成
	void CreateMaterialAndTextureView();
	//座標変換用ビューの生成
	HRESULT CreateTransformView();

	bool LoadPMD(const char* filepath);

public:
	PMDActor(const char* filepath);
	~PMDActor();
	PMDActor* Clone();
	void Update();
	void Draw();
};

