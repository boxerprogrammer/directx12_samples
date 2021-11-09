#include "Application.h"
#include"PMDActor.h"
#include"Dx12Wrapper.h"
#include"PMDRenderer.h"
#include<Effekseer.h>
#include<EffekseerRendererDX12.h>
#include"imgui/imgui.h"
#include"imgui/imgui_impl_win32.h"
#include"imgui/imgui_impl_dx12.h"


//----エフェクトに必要なものの基本--------------
//エフェクトレンダラ
EffekseerRenderer::RendererRef _efkRenderer=nullptr;
//エフェクトマネジャ
Effekseer::ManagerRef _efkManager=nullptr;

//----DX12やVulkan,metalなどのコマンドリスト系への対応のためのもの----
//メモリプール(詳しくは分かってない)
Effekseer::RefPtr<EffekseerRenderer::SingleFrameMemoryPool> _efkMemoryPool = nullptr;
//コマンドリスト(DX12とかVulkanへの対応のため)
Effekseer::RefPtr<EffekseerRenderer::CommandList> _efkCmdList = nullptr;

//----エフェクト再生に必要なもの---------------
//エフェクト本体(エフェクトファイルに対応)
Effekseer::EffectRef _effect=nullptr;
// エフェクトハンドル(再生中のエフェクトに対応)
Effekseer::Handle _efkHandle;

using namespace std;
constexpr int window_width = 1280;
constexpr int window_height = 720;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
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

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heapImgui;

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
	heapImgui= _dx12->CreateDescriptorHeapForImgUi();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(_hwnd);
	ImGui_ImplDX12_Init(_dx12->Device(),
		3,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		heapImgui.Get(),
		heapImgui->GetCPUDescriptorHandleForHeapStart(),
		heapImgui->GetGPUDescriptorHandleForHeapStart());
	

	DXGI_FORMAT bbFormats[] = { DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM };
	_efkRenderer = EffekseerRendererDX12::Create(
		_dx12->Device(),//デバイス
		_dx12->CmdQue(), //コマンドキュー
		2, //バックバッファの数
		bbFormats, //レンダーターゲットフォーマット
		1, //レンダーターゲット数
		DXGI_FORMAT_UNKNOWN, //デプスフォーマット
		false, //反対デプスありか？
		10000);//最大パーティクルの数


	_efkManager = Effekseer::Manager::Create(10000);//最大インスタンス数


	//「系」を左手系にしておく(とにかくクライアント側の系に合わせる)
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
	_efkMemoryPool = EffekseerRenderer::CreateSingleFrameMemoryPool(_efkRenderer->GetGraphicsDevice());
	_efkCmdList = EffekseerRenderer::CreateCommandList(_efkRenderer->GetGraphicsDevice(), _efkMemoryPool);
	_efkRenderer->SetCommandList(_efkCmdList);

	SyncronizeEffekseerCamera();

	_pmdRenderer->Init();


	// エフェクトの読込
	_effect = Effekseer::Effect::Create(_efkManager, (const EFK_CHAR*)L"effect/10/SimpleLaser.efk",1.0f, (const EFK_CHAR*)L"effect/10");

	// エフェクトの再生
	_efkHandle = _efkManager->Play(_effect, 0, 0, 0);

	



	_actor.reset(new PMDActor(_dx12, "Model/初音ミク.pmd"));
	_actor->Move(-10, 0, 10);
	
	_pmdRenderer->AddActor(_actor);
	_pmdRenderer->AddActor("Model/初音ミクmetal.pmd");

	auto satori = make_shared<PMDActor>(_dx12, "Model/鏡音リン.pmd");
	satori->Move(-5, 0, 5);
	_pmdRenderer->AddActor(satori);

	auto ruka = make_shared<PMDActor>(_dx12, "Model/巡音ルカ.pmd");
	ruka->Move(10, 0, 10);
	_pmdRenderer->AddActor(ruka);

	auto hibiki = make_shared<PMDActor>(_dx12, "Model/弱音ハク.pmd");
	hibiki->Move(-10, 0, 0);
	_pmdRenderer->AddActor(hibiki);
	
	auto katu = make_shared<PMDActor>(_dx12, "Model/カイト.pmd");
	katu->Move(10, 0, 0);
	_pmdRenderer->AddActor(katu);


	_pmdRenderer->AnimationStart();
	return true;
}
void Application::SyncronizeEffekseerCamera()
{
	Effekseer::Matrix44 fkViewMat;
	Effekseer::Matrix44 fkProjMat;
	auto view = _dx12->ViewMatrix();
	auto proj = _dx12->ProjMatrix();
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			fkViewMat.Values[i][j] = view.r[i].m128_f32[j];
			fkProjMat.Values[i][j] = proj.r[i].m128_f32[j];
		}
	}
	_efkRenderer->SetCameraMatrix(fkViewMat);
	_efkRenderer->SetProjectionMatrix(fkProjMat);
}
///アプリケーション起動
void 
Application::Run() {
	ShowWindow(_hwnd, SW_SHOW);
	MSG msg = {};
	bool shotFlg = false;
	float fov = 3.1415926535897f / 4.0f;//π/4
	float backcol[4] = { 0.5,0.5,0.5,1 };
	float bloomCol[4] = { 1.0,1.0,1.0,1.0f };
	ImGui::SetNextWindowSize(ImVec2(400, 500));
	bool outline = false;
	bool normalOutline = false;
	bool rimFlg = false;
	bool debugDispFlg = false;
	bool directionalLightFlg = false;
	bool antiAlias = false;
	bool bloomFlg = false;
	bool dofFlg = false;
	bool aoFlg = false;
	int aoTryCount = 1;
	float aoRadius = 1.0f;
	float rimStrength =1.0;
	float focusX=0.5f, focusY=0.5f;
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
		SyncronizeEffekseerCamera();
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
		_dx12->PreDrawToPera1(backcol);
		_dx12->DrawPrimitiveShapes();
		_pmdRenderer->BeforeDraw();
		_dx12->DrawToPera1(_pmdRenderer);
		_pmdRenderer->Draw();

		auto efkpos=_efkManager->GetLocation(_efkHandle);
		efkpos.X += 0.1f;
		_efkManager->SetLocation(_efkHandle, efkpos);

		//エフェクト描画
		_efkManager->Update();//マネージャの更新(時間更新)
		_efkMemoryPool->NewFrame();//適切なバックバッファを選択
		EffekseerRendererDX12::BeginCommandList(_efkCmdList, _dx12->CmdList());//
		_efkRenderer->BeginRendering();//描画前処理
		_efkManager->Draw();//エフェクト描画
		_efkRenderer->EndRendering();//描画後処理
		EffekseerRendererDX12::EndCommandList(_efkCmdList);

		//縮小バッファへ描画
		_dx12->DrawToShrinkBuffer();

		//2枚目
		_dx12->DrawAmbientOcclusion();

		//3枚目
		_dx12->Clear();
		_dx12->Draw(_pmdRenderer);

		//IMGUI用処理
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Rendering Test Menu");
		ImGui::Checkbox("DebugDraw on/off", &debugDispFlg);
		ImGui::Checkbox("Outline on/off", &outline);
		ImGui::Checkbox("NormalOutline on/off", &normalOutline);
		ImGui::Checkbox("RimLight on/off", &rimFlg);
		ImGui::SliderFloat("RimStrength ", &rimStrength,0.1f,20.0f);
		ImGui::Checkbox("DirectionalLight", &directionalLightFlg);
		ImGui::Checkbox("Antialias", &antiAlias);
		ImGui::Checkbox("Bloom", &bloomFlg);
		ImGui::Checkbox("DepthOfField", &dofFlg);
		ImGui::Checkbox("SSAO", &aoFlg);
		ImGui::SliderInt("AO Try Count", &aoTryCount,1,512);
		ImGui::SliderFloat("AO Radius", &aoRadius,0.1f,10.0f);


		ImGui::ColorPicker4("BloomColor", bloomCol);

		ImGui::End();

		ImGui::Render();

		_dx12->SetOutline(outline);
		_dx12->SetNormalOutline(normalOutline);
		_dx12->SetRimLight(rimFlg,rimStrength);
		_dx12->SetDebugDisplay(debugDispFlg);
		_dx12->SetDirectionalLight(directionalLightFlg);
		_dx12->SetAA(antiAlias);
		_dx12->SetBloom(bloomFlg);
		_dx12->SetBloomColor(bloomCol);
		_dx12->SetDOF(dofFlg);
		_dx12->SetAO(aoFlg);
		_dx12->SetAOTryCount(aoTryCount);
		_dx12->SetAORadius(aoRadius);

		if (keycode[VK_LBUTTON] & 0x80) {
			POINT pnt;
			GetCursorPos(&pnt);
			ScreenToClient(_hwnd, &pnt);
			_dx12->SetFocusPos((float)pnt.x, (float)pnt.y);
		}

		_dx12->CmdList()->SetDescriptorHeaps(1, heapImgui.GetAddressOf());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), _dx12->CmdList());



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