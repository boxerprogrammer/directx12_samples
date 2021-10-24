#pragma once

#include<d3d12.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<unordered_map>
#include<wrl.h>

class Dx12Wrapper;
class PMDRenderer;
class PMDActor
{
	friend PMDRenderer;
private:
	unsigned int _duration = 0;
	PMDRenderer& _renderer;
	Dx12Wrapper& _dx12;
	DirectX::XMMATRIX _localMat;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	
	//頂点関連
	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	ComPtr<ID3D12Resource> _transformMat = nullptr;//座標変換行列(今はワールドのみ)
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;//座標変換ヒープ

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

	struct Transform {
		//内部に持ってるXMMATRIXメンバが16バイトアライメントであるため
		//Transformをnewする際には16バイト境界に確保する
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	Transform _transform;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	//マテリアル関連
	std::vector<Material> _materials;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	//ボーン関連
	std::vector<DirectX::XMMATRIX> _boneMatrices;

	struct BoneNode {
		uint32_t boneIdx;//ボーンインデックス
		uint32_t boneType;//ボーン種別
		uint32_t parentBone;
		uint32_t ikParentBone;//IK親ボーン
		DirectX::XMFLOAT3 startPos;//ボーン基準点(回転中心)
		std::vector<BoneNode*> children;//子ノード
	};
	std::unordered_map<std::string, BoneNode> _boneNodeTable;
	std::vector<std::string> _boneNameArray;//インデックスから名前を検索しやすいようにしておく
	std::vector<BoneNode*> _boneNodeAddressArray;//インデックスからノードを検索しやすいようにしておく


	struct PMDIK {
		uint16_t boneIdx;//IK対象のボーンを示す
		uint16_t targetIdx;//ターゲットに近づけるためのボーンのインデックス
		uint16_t iterations;//試行回数
		float limit;//一回当たりの回転制限
		std::vector<uint16_t> nodeIdxes;//間のノード番号
	};
	std::vector<PMDIK> _ikData;
	
	//読み込んだマテリアルをもとにマテリアルバッファを作成
	HRESULT CreateMaterialData();
	
	ComPtr< ID3D12DescriptorHeap> _materialHeap = nullptr;//マテリアルヒープ(5個ぶん)
	//マテリアル＆テクスチャのビューを作成
	HRESULT CreateMaterialAndTextureView();

	//座標変換用ビューの生成
	HRESULT CreateTransformView();

	//PMDファイルのロード
	HRESULT LoadPMDFile(const char* path);
	void RecursiveMatrixMultipy(BoneNode* node, const DirectX::XMMATRIX& mat,bool flg=false);
	float _angle;//テスト用Y軸回転


	///キーフレーム構造体
	struct KeyFrame {
		unsigned int frameNo;//フレーム№(アニメーション開始からの経過時間)
		DirectX::XMVECTOR quaternion;//クォータニオン
		DirectX::XMFLOAT3 offset;//IKの初期座標からのオフセット情報
		DirectX::XMFLOAT2 p1, p2;//ベジェの中間コントロールポイント
		KeyFrame(
			unsigned int fno, 
			const DirectX::XMVECTOR& q,
			const DirectX::XMFLOAT3& ofst,
			const DirectX::XMFLOAT2& ip1,
			const DirectX::XMFLOAT2& ip2):
			frameNo(fno),
			quaternion(q),
			offset(ofst),
			p1(ip1),
			p2(ip2){}
	};
	std::unordered_map<std::string, std::vector<KeyFrame>> _motiondata;

	float GetYFromXOnBezier(float x,const DirectX::XMFLOAT2& a,const DirectX::XMFLOAT2& b, uint8_t n = 12);
	
	std::vector<uint32_t> _kneeIdxes;

	DWORD _startTime;//アニメーション開始時点のミリ秒時刻
	
	void MotionUpdate();

	///CCD-IKによりボーン方向を解決
	///@param ik 対象IKオブジェクト
	void SolveCCDIK(const PMDIK& ik);

	///余弦定理IKによりボーン方向を解決
	///@param ik 対象IKオブジェクト
	void SolveCosineIK(const PMDIK& ik);
	
	///LookAt行列によりボーン方向を解決
	///@param ik 対象IKオブジェクト
	void SolveLookAt(const PMDIK& ik);

	void IKSolve(int frameNo);

	//IKオンオフデータ
	struct VMDIKEnable {
		uint32_t frameNo;
		std::unordered_map<std::string, bool> ikEnableTable;
	};
	std::vector<VMDIKEnable> _ikEnableData;

public:
	PMDActor(const char* filepath,PMDRenderer& renderer);
	~PMDActor();
	///クローンは頂点およびマテリアルは共通のバッファを見るようにする
	PMDActor* Clone();
	void LoadVMDFile(const char* filepath, const char* name);
	void Update();
	void Draw();
	void PlayAnimation();

	void LookAt(float x,float y, float z);
};

