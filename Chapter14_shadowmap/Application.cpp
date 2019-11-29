#include "Application.h"
#include"PMDActor.h"
#include"Dx12Wrapper.h"
#include"PMDRenderer.h"
#include<Effekseer.h>
#include<EffekseerRendererDX12.h>

//----エフェクトに必要なものの基本--------------
//エフェクトレンダラ
EffekseerRenderer::Renderer* _efkRenderer=nullptr;
//エフェクトマネジャ
Effekseer::Manager* _efkManager=nullptr;

//----DX12やVulkan,metalなどのコマンドリスト系への対応のためのもの----
//メモリプール(詳しくは分かってない)
EffekseerRenderer::SingleFrameMemoryPool* _efkMemoryPool = nullptr;
//コマンドリスト(DX12とかVulkanへの対応のため)
EffekseerRenderer::CommandList* _efkCmdList = nullptr;

//----エフェクト再生に必要なもの---------------
//エフェクト本体(エフェクトファイルに対応)
Effekseer::Effect* _effect=nullptr;
// エフェクトハンドル(再生中のエフェクトに対応)
Effekseer::Handle _efkHandle;

using namespace std;
constexpr int window_width = 1280;
constexpr int window_height = 720;

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

Application& 
Application::Instance() {
	static Application instance;
	return instance;
}

Application::Application()
{
}


Application::~Application()
{
}


bool 
Application::Initialize() {

	auto result=CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	
	_wndClass.hInstance = GetModuleHandle(nullptr);
	_wndClass.cbSize = sizeof(WNDCLASSEX);
	_wndClass.lpfnWndProc = (WNDPROC)WindowProcedure;
	_wndClass.lpszClassName = "DirectX12サンプル";
	RegisterClassEx(&_wndClass);
	
	RECT wrc = {};
	wrc.left= 0;
	wrc.top = 0;
	wrc.right = window_width;
	wrc.bottom = window_height;
	AdjustWindowRect(&wrc,WS_OVERLAPPEDWINDOW, false);


	_hwnd = CreateWindow(
		_wndClass.lpszClassName,
		"DirectX12の実験でーす",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom-wrc.top,
		nullptr,
		nullptr,
		_wndClass.hInstance,
		nullptr
	);


	if (_hwnd == 0)return false;


	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(_dx12));
	if (!_dx12->Init()) {
		return false;
	}

	DXGI_FORMAT bbFormats[] = { DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM };
	_efkRenderer = EffekseerRendererDX12::Create(
		_dx12->Device(),//デバイス
		_dx12->CmdQue(), //コマンドキュー
		2, //バックバッファの数
		bbFormats, //レンダーターゲットフォーマット
		1, //レンダーターゲット数
		false, //デプスありか？
		false, //反対デプスありか？
		2000);//パーティクルの数

	_efkManager = Effekseer::Manager::Create(2000);


	//「系」を左手系にしておく
	_efkManager->SetCoordinateSystem(Effekseer::CoordinateSystem::LH);


	// 描画用インスタンスから描画機能を設定
	_efkManager->SetSpriteRenderer(_efkRenderer->CreateSpriteRenderer());
	_efkManager->SetRibbonRenderer(_efkRenderer->CreateRibbonRenderer());
	_efkManager->SetRingRenderer(_efkRenderer->CreateRingRenderer());
	_efkManager->SetTrackRenderer(_efkRenderer->CreateTrackRenderer());
	_efkManager->SetModelRenderer(_efkRenderer->CreateModelRenderer());

	// 描画用インスタンスからテクスチャの読込機能を設定
	// 独自拡張可能、現在はファイルから読み込んでいる。
	_efkManager->SetTextureLoader(_efkRenderer->CreateTextureLoader());
	_efkManager->SetModelLoader(_efkRenderer->CreateModelLoader());

	//DX12特有の処理
	_efkMemoryPool = EffekseerRendererDX12::CreateSingleFrameMemoryPool(_efkRenderer);
	_efkCmdList = EffekseerRendererDX12::CreateCommandList(_efkRenderer, _efkMemoryPool);
	_efkRenderer->SetCommandList(_efkCmdList);

	// 投影行列を設定
	_efkRenderer->SetProjectionMatrix(
		::Effekseer::Matrix44().PerspectiveFovLH(90.0f / 180.0f * 3.14f, (float)1280 / (float)720, 1.0f, 50.0f));

	// カメラ行列を設定
	_efkRenderer->SetCameraMatrix(
		::Effekseer::Matrix44().LookAtLH(Effekseer::Vector3D(0.0f, 5.0f, -25.0f), ::Effekseer::Vector3D(0.0f, 5.0f, 0.0f), ::Effekseer::Vector3D(0.0f, 1.0f, 0.0f)));
	_pmdRenderer->Init();


	// エフェクトの読込
	_effect = Effekseer::Effect::Create(_efkManager, (const EFK_CHAR*)L"effect/suzuki.efk",1.0f, (const EFK_CHAR*)L"effect");

	// エフェクトの再生
	_efkHandle = _efkManager->Play(_effect, 0, 0, 0);

	


	//_actor.reset(new PMDActor("Model/初音ミク.pmd"));
	//_actor.reset(new PMDActor("Model/飛鳥/飛鳥Ver1.10SW.pmd"));
	//_actor.reset(new PMDActor("Model/satori/古明地さとり152Normal.pmd"));
	_actor.reset(new PMDActor(_dx12, "Model/sakura/mikuXS桜ミク.pmd"));
	//_actor.reset(new PMDActor("Model/巡音ルカ.pmd"));
	_actor->Move(-10, 0, 10);
	
	_pmdRenderer->AddActor(_actor);
	_pmdRenderer->AddActor("Model/初音ミクmetal.pmd");

	auto satori = make_shared<PMDActor>(_dx12, "Model/satori/古明地さとり152Normal.pmd");
	satori->Move(-5, 0, 5);
	_pmdRenderer->AddActor(satori);

	auto ruka = make_shared<PMDActor>(_dx12, "Model/巡音ルカ.pmd");
	ruka->Move(10, 0, 10);
	_pmdRenderer->AddActor(ruka);

	//auto satori = make_shared<PMDActor>(_dx12, "Model/satori/古明地さとり152Normal.pmd");
	//satori->Move(10, 0, 0);
	//_pmdRenderer->AddActor(satori);

	auto hibiki = make_shared<PMDActor>(_dx12, "Model/hibiki/我那覇響v1.pmd");
	hibiki->Move(-10, 0, 0);
	_pmdRenderer->AddActor(hibiki);
	
	auto katu = make_shared<PMDActor>(_dx12, "Model/ikaruga/斑鳩Ver1.10SW.pmd");
	katu->Move(10, 0, 0);
	_pmdRenderer->AddActor(katu);
	//_dx12->AddPMDActor(&*_actor);


	_pmdRenderer->AnimationStart();
	return true;
}
///アプリケーション起動
void 
Application::Run() {
	ShowWindow(_hwnd, SW_SHOW);
	MSG msg = {};
	bool shotFlg = false;
	float fov = 3.1415926535897f / 4.0f;//π/4
	while (true) {//メインループ
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);//翻訳
			DispatchMessage(&msg);//
		}
		if (msg.message == WM_QUIT) {
			break;
		}
		BYTE keycode[256];
		GetKeyboardState(keycode);
		float x=0, y=0, z = 0;
		
		if (keycode[VK_RIGHT]&0x80) {
			x += 0.1f;
		}
		if (keycode[VK_LEFT] & 0x80) {
			x -= 0.1f;
		}
		if (keycode[VK_UP] & 0x80) {
			y += 0.1f;
		}
		if (keycode[VK_DOWN] & 0x80) {
			y -= 0.1f;
		}
		if (keycode['Z'] & 0x80) {
			fov += 0.01f;
		}
		if (keycode['X'] & 0x80) {
			fov -= 0.01f;
		}

		float px=0, py=0, pz=0;
		if (keycode['W'] & 0x80) {
			pz += 0.1f;
		}
		if (keycode['A'] & 0x80) {
			px-= 0.1f;
		}
		if (keycode['S'] & 0x80) {
			pz -= 0.1f;
		}
		if (keycode['D'] & 0x80) {
			px += 0.1f;
		}
		if (keycode['R'] & 0x80) {
			_actor->Rotate(0, 0.01f, 0);
		}
		if (keycode['T'] & 0x80) {
			_actor->Rotate(0.01f,0, 0);
		}
		if (keycode['Y'] & 0x80) {
			_actor->Rotate(0, 0, 0.01f);
		}
		if (keycode[VK_SPACE] & 0x80) {
			if (!shotFlg) {
				if (_efkManager->Exists(_efkHandle)) {
					_efkManager->StopEffect(_efkHandle);
				}
				// エフェクトの再生
				_efkHandle = _efkManager->Play(_effect, 0, 0, 0);
			}
			shotFlg = true;
		}
		else {
			shotFlg = false;
		}

		_actor->Move(px, py, pz);
		
		_dx12->MoveEyePosition(x, y, z);
		_dx12->SetFov(fov);
		_dx12->SetCameraSetting();

		_pmdRenderer->Update();

		_pmdRenderer->BeforeDrawFromLight();
		//影への描画
		_dx12->PreDrawShadow();
		_pmdRenderer->DrawFromLight();



		//１枚目(ペラポリへ)
		_dx12->PreDrawToPera1();
		_dx12->DrawPrimitiveShapes();
		_pmdRenderer->BeforeDraw();
		_dx12->DrawToPera1(_pmdRenderer);
		_pmdRenderer->Draw();

		//エフェクト描画
		_efkManager->Update();//マネージャの更新(時間更新)
		_efkMemoryPool->NewFrame();//適切なバックバッファを選択
		EffekseerRendererDX12::BeginCommandList(_efkCmdList, _dx12->CmdList());//
		_efkRenderer->BeginRendering();//描画前処理
		_efkManager->Draw();//エフェクト描画
		_efkRenderer->EndRendering();//描画後処理
		EffekseerRendererDX12::EndCommandList(_efkCmdList);
		
		//2枚目(ペラポリ1→ペラポリ2へ)
		//_dx12->DrawToPera2();

		//3枚目(ペラポリ2→バックバッファへ)
		_dx12->Clear();
		_dx12->Draw(_pmdRenderer);
		_dx12->Flip();
	}
}
///アプリケーション終了
void 
Application::Terminate() {
	CoUninitialize();
	UnregisterClass(_wndClass.lpszClassName, _wndClass.hInstance);
}

Size 
Application::GetWindowSize() {
	return Size(window_width,window_height);
}