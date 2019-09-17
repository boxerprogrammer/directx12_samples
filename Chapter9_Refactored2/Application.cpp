//コンスタントバッファで行列を転送


#include "Application.h"
#include"Dx12Wrapper.h"
#include"PMDRenderer.h"
#include"PMDActor.h"

#ifdef _DEBUG
#include<iostream>
#endif

///@brief コンソール画面にフォーマット付き文字列を表示
///@param format フォーマット(%dとか%fとかの)
///@param 可変長引数
///@remarksこの関数はデバッグ用です。デバッグ時にしか動作しません
void DebugOutputFormatString(const char* format, ...) {
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

//面倒だけど書かなあかんやつ
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) {//ウィンドウが破棄されたら呼ばれます
		PostQuitMessage(0);//OSに対して「もうこのアプリは終わるんや」と伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);//規定の処理を行う
}

//ウィンドウ定数
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

void 
Application::CreateGameWindow(HWND &hwnd, WNDCLASSEX &windowClass) {
	HINSTANCE hInst = GetModuleHandle(nullptr);
	//ウィンドウクラス生成＆登録
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;//コールバック関数の指定
	windowClass.lpszClassName = _T("DirectXTest");//アプリケーションクラス名(適当でいいです)
	windowClass.hInstance = GetModuleHandle(0);//ハンドルの取得
	RegisterClassEx(&windowClass);//アプリケーションクラス(こういうの作るからよろしくってOSに予告する)

	RECT wrc = { 0,0, window_width, window_height };//ウィンドウサイズを決める
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);//ウィンドウのサイズはちょっと面倒なので関数を使って補正する
	//ウィンドウオブジェクトの生成
	hwnd = CreateWindow(windowClass.lpszClassName,//クラス名指定
		_T("DX12リファクタリング"),//タイトルバーの文字
		WS_OVERLAPPEDWINDOW,//タイトルバーと境界線があるウィンドウです
		CW_USEDEFAULT,//表示X座標はOSにお任せします
		CW_USEDEFAULT,//表示Y座標はOSにお任せします
		wrc.right - wrc.left,//ウィンドウ幅
		wrc.bottom - wrc.top,//ウィンドウ高
		nullptr,//親ウィンドウハンドル
		nullptr,//メニューハンドル
		windowClass.hInstance,//呼び出しアプリケーションハンドル
		nullptr);//追加パラメータ

}



void 
Application::Run() {
	ShowWindow(_hwnd, SW_SHOW);//ウィンドウ表示
	float angle = 0.0f;
	MSG msg = {};
	unsigned int frame = 0;
	while (true) {
		//_worldMat = XMMatrixRotationY(angle);
		//_mapScene->world = _worldMat;
		//_mapScene->view = _viewMat;
		//_mapScene->proj = _projMat;
		//angle += 0.01f;

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT) {
			break;
		}

		//全体の描画準備
		_dx12->BeginDraw();

		//PMD用の描画パイプラインに合わせる
		_dx12->CommandList()->SetPipelineState(_pmdRenderer->GetPipelineState());
		//ルートシグネチャもPMD用に合わせる
		_dx12->CommandList()->SetGraphicsRootSignature(_pmdRenderer->GetRootSignature());

		_dx12->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
		
		_pmdActor->Draw();





		_dx12->EndDraw();

		//フリップ
		_dx12->Swapchain()->Present(1, 0);

	}
}

bool 
Application::Init() {
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	CreateGameWindow(_hwnd, _windowClass);


	//DirectX12ラッパー生成＆初期化
	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(*_dx12));
	_pmdActor.reset(new PMDActor("Model/初音ミク.pmd", *_pmdRenderer));

//#ifdef _DEBUG
//	//デバッグレイヤーをオンに
//	EnableDebugLayer();
//#endif
//
//	//DirectX12関連初期化
//	if (FAILED(InitializeDXGIDevice())) {
//		assert(0);
//		return false;
//	}
//	if (FAILED(InitializeCommand())) {
//		assert(0);
//		return false;
//	}
//	if (FAILED(CreateSwapChain(_hwnd, _dxgiFactory))) {
//		assert(0);
//		return false;
//	}
//	if (FAILED(CreateFinalRenderTarget(_rtvHeaps, _backBuffers))) {
//		assert(0);
//		return false;
//	}
//
//	//テクスチャローダー関連初期化
//	CreateTextureLoaderTable();


	////深度バッファ作成
	//if (FAILED(CreateDepthStencilView())) {
	//	assert(0);
	//	return false;
	//}

	////フェンスの作成
	//if (FAILED(_dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf())))) {
	//	assert(0);
	//	return false;
	//}


	//_whiteTex = CreateWhiteTexture();
	//_blackTex = CreateBlackTexture();
	//_gradTex = CreateGrayGradationTexture();

	//LoadPMDFile("Model/hibiki/hibiki.pmd");
	//LoadPMDFile("Model/satori/satori.pmd");
	//LoadPMDFile("Model/reimu/reimu.pmd");
	//LoadPMDFile("Model/巡音ルカ.pmd");
	//if (FAILED(LoadPMDFile("Model/初音ミク.pmd"))) {
	//	return false;
	//}

	////ロードしたデータをもとにバッファにマテリアルデータを作る
	//if (FAILED(CreateMaterialData())) {
	//	return false;
	//}

	////マテリアルまわりのビュー作成およびテクスチャビュー作成
	////同じデスクリプタヒープ内に作っていくためマテリアルと
	////強い結合状態になっている。
	//CreateMaterialAndTextureView();

	////ルートシグネチャ作成
	//if (FAILED(CreateRootSignature())) {
	//	return false;
	//}

	////パイプライン設定(シェーダもここで設定)
	//if (FAILED(CreateBasicGraphicsPipeline())) {
	//	return false;
	//}

	//if (FAILED(CreateSceneTransformView())) {
	//	return false;
	//}
	return true;
}

void
Application::Terminate() {
	//もうクラス使わんから登録解除してや
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}


Application::Application()
{
}


Application::~Application()
{
}

Application& 
Application::Instance() {
	static Application instance;
	return instance;
}

