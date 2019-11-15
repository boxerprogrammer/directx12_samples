#pragma once
#include<Windows.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXTex.h>
#include<vector>
#include<DirectXMath.h>
#include<wrl.h>
#include<unordered_map>
#include<memory>

class PMDActor;
class PMDRenderer;
using Microsoft::WRL::ComPtr;
///DirectX12の各要素とか関数を
///ラップしてるだけのクラス
class Dx12Wrapper
{
private:
	struct MultiTexture{
		ComPtr<ID3D12Resource> texBuff;//通常テクスチャバッファ
		ComPtr<ID3D12Resource> sphBuff;//SPHテクスチャ
		ComPtr<ID3D12Resource> spaBuff;//SPAテクスチャ
		ComPtr<ID3D12Resource> toonBuff;//トゥーンテクスチャ
	};

	HWND _hwnd;

	//基本的な奴(DXGI)
	ComPtr < IDXGIFactory4> _dxgiFactory;
	ComPtr < IDXGISwapChain4> _swapchain;

	//基本的な奴(デバイス)
	ComPtr < ID3D12Device> _dev;


	//コマンドキュー(コマンド実行の単位)
	ComPtr < ID3D12CommandQueue> _cmdQue;

	//深度バッファ用バッファ
	ComPtr<ID3D12Resource> _depthBuffer;
	//深度バッファビュー用スクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> _dsvHeap;

	bool CreateDepthBuffer();
	bool CreateDSV();

	//レンダーターゲットビュー用デスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> _rtvDescHeap;
	//スワップチェインが持っているリソースへのポインタ
	std::vector<ID3D12Resource*> _backBuffers;

	//コマンドリストを格納するためのメモリ領域
	ComPtr <ID3D12CommandAllocator> _cmdAlloc = nullptr;
	//コマンドリスト本体(コマンドアロケータに命令を登録するための
	//インターフェイス)
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	
	//待ちのためのフェンス
	ComPtr<ID3D12Fence> _fence;
	UINT64 _fenceValue;

	//無駄読みしないようにテクスチャのテーブルを作っておく
	std::unordered_map<std::wstring, ComPtr<ID3D12Resource>> _textureTable;

	bool CreateTextureFromImageData(const DirectX::Image* img, ComPtr<ID3D12Resource>& buff,bool isDiscrete=false);

	ComPtr<ID3D12Resource> _whiteTex;//白テクスチャ
	///乗算用の真っ白テクスチャ(乗算されても影響が出ない)を作ります。
	bool CreateWhiteTexture();

	ComPtr<ID3D12Resource> _blackTex;//黒テクスチャ
	///加算用の真っ黒テクスチャ(加算されても影響が出ない)を作ります。
	bool CreateBlackTexture();

	ComPtr<ID3D12Resource> _gradTex;//グレーグラデーションテクスチャ
	//トゥーンがない場合の階調を定義する
	bool CreateGradationTexture();

	ComPtr < ID3D12Resource> _sceneCB;//座標変換定数バッファ
	ComPtr < ID3D12DescriptorHeap> _sceneHeap;//座標変換CBVヒープ
	///座標変換用定数バッファおよび定数バッファビューを作成する
	bool CreateTransformConstantBuffer();
	bool CreateTransformBufferView();

	struct SceneMatrix {
		DirectX::XMMATRIX view;//ビュー
		DirectX::XMMATRIX proj;//プロジェクション
		DirectX::XMMATRIX shadow;//影
		DirectX::XMFLOAT3 eye;//視点
	};
	SceneMatrix* _mappedScene;

	//視点(カメラの位置)XMVECTOR
	//注視点(見る対象の位置)XMVECTOR
	//上ベクトル(上)XMVECTOR
	DirectX::XMFLOAT3 _eye;
	DirectX::XMFLOAT3 _target;
	DirectX::XMFLOAT3 _up;
	//平行ライトの向き
	DirectX::XMFLOAT3 _parallelLightVec;

	float _fov = DirectX::XM_PI/6;//デフォルト30°

	bool CreateCommandList();
	void Barrier(ID3D12Resource* p,
		D3D12_RESOURCE_STATES before, 
		D3D12_RESOURCE_STATES after);

	//歪みテクスチャ用
	ComPtr<ID3D12DescriptorHeap> _distortionSRVHeap;
	ComPtr<ID3D12Resource> _distortionTexBuffer;
	bool CreateEffectBufferAndView();


	//1枚目レンダリング用
	//いわゆるペラポリに張り付けるための絵の
	//メモリリソースとそのビュー
	ComPtr<ID3D12DescriptorHeap> _peraRTVHeap;
	ComPtr<ID3D12DescriptorHeap> _peraRegisterHeap;
	ComPtr<ID3D12Resource> _peraResource;
	//１枚目ペラポリのためのリソースとビューを
	//作成
	bool CreatePeraResourcesAndView();

	ComPtr<ID3D12Resource> _bokehParamResource;
	//ボケに関するバッファを作り中にボケパラメータを代入する
	bool CreateBokehParamResource();

	//ペラポリ2枚目
	ComPtr<ID3D12Resource> _peraResource2;
	ComPtr<ID3D12PipelineState> _peraPipeline2;

	//ペラポリ用頂点バッファ(N字の4点)
	ComPtr<ID3D12Resource> _peraVB;
	D3D12_VERTEX_BUFFER_VIEW _peraVBV;

	//ペラポリ用パイプライン＆ルートシグネチャ
	ComPtr<ID3D12PipelineState> _peraPipeline;
	ComPtr<ID3D12RootSignature> _peraRS;

	bool CreatePeraVertex();
	bool CreatePeraPipeline();

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	ID3D12Device* Device() {
		return _dev.Get();
	}
	ID3D12GraphicsCommandList* CmdList() {
		return _cmdList.Get();
	}

	bool Init();
	bool CreateRenderTargetView();

	ComPtr<ID3D12Resource> WhiteTexture();
	ComPtr<ID3D12Resource> BlackTexture();
	ComPtr<ID3D12Resource> GradTexture();


	//ペラポリゴンへの描画準備
	bool PreDrawToPera1();
	//ペラポリゴンへの描画後処理
	void PostDrawToPera1();

	//ペラポリゴンへの描画
	void DrawToPera1(std::shared_ptr<PMDRenderer> renderer);

	//画面のクリア
	bool Clear();

	//描画
	void Draw(std::shared_ptr<PMDRenderer> renderer);

	void DrawHorizontalBokeh();

	void SetCameraSetting();

	//フリップ
	void Flip();
	void WaitForCommandQueue();

	bool LoadPictureFromFile(std::wstring filepath, ComPtr<ID3D12Resource>& buff);

	void SetFov(float angle);
	void SetEyePosition(float x, float y, float z);
	void MoveEyePosition(float x, float y, float z);

	DirectX::XMVECTOR GetCameraPosition();

};

