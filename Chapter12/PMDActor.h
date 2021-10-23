#pragma once

#include<vector>
#include<array>
#include<string>
#include<DirectXMath.h>
#include<d3d12.h>
#include<wrl.h>
#include<map>
#include<unordered_map>
#include<memory>
#include<algorithm>

using Microsoft::WRL::ComPtr;
class Dx12Wrapper;
struct Material{
	DirectX::XMFLOAT4 diffuse;//ディフューズ色
	float power;//スペキュラ強さ
	DirectX::XMFLOAT3 specular;//スペキュラ色
	DirectX::XMFLOAT3 ambient;//環境色
	uint32_t indicesNum;//インデックス数
};

struct MultiTexturePath {
	std::string texPath;//通常テクスチャパス
	std::string sphPath;//乗算テクスチャパス
	std::string spaPath;//加算テクスチャパス

	std::string toonPath;//トゥーンテクスチャパス

};

class PMDActor
{
private:
	//モーション情報
	struct KeyFrame {
		uint32_t frameNo;//キーフレームがある経過フレーム数
		DirectX::XMFLOAT4 quaternion;//そのときどれくらい回転させるのか
		DirectX::XMFLOAT3 offset;//元の位置からのオフセット
		std::array<DirectX::XMFLOAT2,2> cpnt;//コントロールポイント
		KeyFrame() {}
		KeyFrame(uint32_t fno, DirectX::XMFLOAT4& q,DirectX::XMFLOAT3& ofst,float cx1,float cy1,float cx2,float cy2) :frameNo(fno), 
			quaternion(q),
			offset(ofst){
			cpnt[0].x = cx1;
			cpnt[0].y = cy1;
			cpnt[1].x = cx2;
			cpnt[1].y = cy2;
		}
	};
	std::unordered_map<std::string, std::vector<KeyFrame>> _keyframes;
	uint32_t _duration;//アニメーションの総フレーム数

	std::vector<uint32_t> _eyeBoneIdxes;

	//ロード等の処理が終わってアニメーションを開始した時点での
	//TickCount←PC起動時からのミリ秒
	uint32_t _lastTickCount;

	//現在の経過フレーム数に従って
	//ボーン行列を更新する
	void UpdateMotion(uint32_t frame);

	//ボーンの情報
	struct BoneInfo {
		int index;//自分のインデックス
		DirectX::XMFLOAT3 pos;//ボーン中心座標
		BoneInfo(int idx, DirectX::XMFLOAT3& inpos) :index(idx), pos(inpos) {}
		BoneInfo() :index(0), pos(DirectX::XMFLOAT3()) {}
	};
	std::vector<BoneInfo*> _boneAddressArray;
	std::map<std::string, BoneInfo> _boneTable;
	std::vector<DirectX::XMMATRIX> _boneMatrices;//最終的にグラボに渡すデータ
	std::vector< std::vector<int> > _boneTree;//ボーンツリー
	ComPtr<ID3D12Resource> _bonesBuff;//ボーン配列用バッファ
	DirectX::XMMATRIX* _mappedBoneMatrix;
	bool CreateBoneBuffer();

	DirectX::XMFLOAT3 _rotator;
	DirectX::XMFLOAT3 _pos;

	unsigned int _vertNum;
	unsigned int _indexNum;

	std::vector<uint8_t> _vertexData;// 頂点データ
	std::vector<uint16_t> _indexData;// インデックスデータ
	std::vector<Material> _materials;// マテリアルデータ
	std::vector<MultiTexturePath> _texturePaths;//テクスチャの相対パス
	bool LoadFromPMD(const char* filepath);

	ComPtr<ID3D12Resource> _vertexBuff;//頂点バッファ
	ComPtr<ID3D12Resource> _indexBuff;//インデックスバッファ
	ComPtr<ID3D12Resource> _materialBuff;//マテリアルバッファ
	struct CompositeTexture {
		ComPtr<ID3D12Resource> tex;//通常
		ComPtr<ID3D12Resource> sph;//乗算スフィアマップ
		ComPtr<ID3D12Resource> spa;//加算スフィアマップ
		ComPtr<ID3D12Resource> toon;//トゥーン
	};
	std::vector<CompositeTexture> _texBuff;//テクスチャバッファ(通常/SPH/SPA/TOON)

	//ビュー
	D3D12_VERTEX_BUFFER_VIEW _vbView;//頂点バッファ
	D3D12_INDEX_BUFFER_VIEW _ibView;//インデックスバッファ
	ComPtr<ID3D12DescriptorHeap> _materialHeap;//マテリアルひとまとめ(テクスチャも含む)

	//バッファ作る関数
	bool CreateVertexBufferAndView();
	bool CreateIndexBufferAndView();
	bool CreateMaterialBuffer();
	//テクスチャロード
	bool LoadTexture();
	//マテリアルバッファビュー
	bool CreateMaterialBufferView();
	std::shared_ptr<Dx12Wrapper> _dx;

	
	ComPtr < ID3D12Resource> _transformCB;//プレイヤー移動定数バッファ
	ComPtr < ID3D12DescriptorHeap> _transformHeap;//座標変換CBVヒープ
	bool CreateTransformBuffer();
	bool CreateTransformBufferView();

	DirectX::XMMATRIX* _mappedTransform;//


	void RecursiveBoneTransform(int idx,const DirectX::XMMATRIX& mat);

public:
	PMDActor(std::shared_ptr<Dx12Wrapper> dx, const char* path);
	~PMDActor();

	void LoadVMDData(const char* vmdpath);

	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView()const;
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView()const;

	ComPtr<ID3D12Resource> GetMaterialBuffer();
	unsigned int GetMaterialNum()const;
	ComPtr<ID3D12DescriptorHeap> GetMaterialAndTextureView();

	ComPtr<ID3D12Resource> GetTransformBuffer();
	ComPtr<ID3D12DescriptorHeap> GetTransformBufferView();

	std::vector<uint8_t>& GetVertexData();
	unsigned int GetVertexNum()const;

	std::vector<uint16_t>& GetIndexData();
	unsigned int GetIndexNum()const;

	std::vector<Material>& Materials();

	std::vector<MultiTexturePath>& GetTexturePaths();

	void Move(float x, float y, float z);
	void Rotate(float x, float y, float z);

	const DirectX::XMFLOAT3& GetPosition()const;
	const DirectX::XMFLOAT3& GetRotate()const;
	void Update();
	void Draw();
	void StartAmimation();

};