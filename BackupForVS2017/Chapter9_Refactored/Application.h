#pragma once
#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<d3dcompiler.h>
#include<DirectXTex.h>
#include<d3dx12.h>
#include<wrl.h>

///シングルトンクラス
class Application
{
private:
	//ここに必要な変数(バッファやヒープなど)を書く
	//ウィンドウ周り
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	//DXGIまわり
	Microsoft::WRL::ComPtr < IDXGIFactory6> _dxgiFactory = nullptr;//DXGIインターフェイス
	Microsoft::WRL::ComPtr < IDXGISwapChain4> _swapchain = nullptr;//スワップチェイン

	//DirectX12まわり
	Microsoft::WRL::ComPtr< ID3D12Device> _dev = nullptr;//デバイス
	Microsoft::WRL::ComPtr < ID3D12CommandAllocator> _cmdAllocator = nullptr;//コマンドアロケータ
	Microsoft::WRL::ComPtr < ID3D12GraphicsCommandList> _cmdList = nullptr;//コマンドリスト
	Microsoft::WRL::ComPtr < ID3D12CommandQueue> _cmdQueue = nullptr;//コマンドキュー

	//必要最低限のバッファまわり
	Microsoft::WRL::ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _vertBuff = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _idxBuff = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constBuff = nullptr;

	//ロード用テーブル
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map < std::string, LoadLambda_t> _loadLambdaTable;

	//マテリアル周り
	unsigned int _materialNum;//マテリアル数
	Microsoft::WRL::ComPtr<ID3D12Resource> _materialBuff = nullptr;
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};

	//デフォルトのテクスチャ(白、黒、グレイスケールグラデーション)
	Microsoft::WRL::ComPtr<ID3D12Resource> _whiteTex = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _blackTex = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _gradTex = nullptr;

	//座標変換系行列
	DirectX::XMMATRIX _worldMat;
	DirectX::XMMATRIX _viewMat;
	DirectX::XMMATRIX _projMat;

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
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> _textureResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> _sphResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> _spaResources;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> _toonResources;

	//シェーダ側に渡すための基本的な環境データ
	struct SceneData {
		DirectX::XMMATRIX world;//ワールド行列
		DirectX::XMMATRIX view;//ビュープロジェクション行列
		DirectX::XMMATRIX proj;//
		DirectX::XMFLOAT3 eye;//視点座標
	};
	SceneData* _mapScene;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _basicDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _materialDescHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;

	//頂点＆インデックスバッファビュー
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	//ファイル名パスとリソースのマップテーブル
	std::map<std::string, ID3D12Resource*> _resourceTable;

	//パイプライン＆ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelinestate = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootsignature = nullptr;

	std::vector<ID3D12Resource*> _backBuffers;//バックバッファ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;//レンダーターゲット用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;//深度バッファビュー用デスクリプタヒープ
	CD3DX12_VIEWPORT _viewport;//ビューポート
	CD3DX12_RECT _scissorrect;//シザー矩形

	//テクスチャバッファ周り
	ID3D12Resource* CreateWhiteTexture();//白テクスチャの生成
	ID3D12Resource*	CreateBlackTexture();//黒テクスチャの生成
	ID3D12Resource*	CreateGrayGradationTexture();//グレーテクスチャの生成
	ID3D12Resource*	LoadTextureFromFile(std::string& texPath);//指定テクスチャのロード

	//最終的なレンダーターゲットの生成
	HRESULT	CreateFinalRenderTarget(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& rtvHeaps, std::vector<ID3D12Resource *>& backBuffers);

	//スワップチェインの生成
	HRESULT CreateSwapChain(const HWND &hwnd, Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory);

	//ゲーム用ウィンドウの生成
	void CreateGameWindow(HWND &hwnd, WNDCLASSEX &windowClass);

	//DXGIまわり初期化
	HRESULT InitializeDXGIDevice();

	//コマンドまわり初期化
	HRESULT InitializeCommand();

	//パイプライン初期化
	HRESULT CreateBasicGraphicsPipeline();
	//ルートシグネチャ初期化
	HRESULT CreateRootSignature();

	//テクスチャローダテーブルの作成
	void CreateTextureLoaderTable();

	//デプスステンシルビューの生成
	HRESULT CreateDepthStencilView();

	//PMDファイルのロード
	HRESULT LoadPMDFile(const char* path);

	//GPU側のマテリアルデータの作成
	HRESULT CreateMaterialData();

	//座標変換用ビューの生成
	HRESULT CreateSceneTransformView();

	//マテリアル＆テクスチャのビューを作成
	void CreateMaterialAndTextureView();

	//↓シングルトンのためにコンストラクタをprivateに
	//さらにコピーと代入を禁止に
	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	///Applicationのシングルトンインスタンスを得る
	static Application& Instance();

	///初期化
	bool Init();

	///ループ起動
	void Run();

	///後処理
	void Terminate();

	~Application();
};

