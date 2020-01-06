#include "Application.h"
#include"PMDActor.h"
#include"Dx12Wrapper.h"
#include"PMDRenderer.h"
#include"imgui/imgui.h"
#include"imgui/imgui_impl_win32.h"
#include"imgui/imgui_impl_dx12.h"


using namespace std;
constexpr int window_width = 1280;
constexpr int window_height = 720;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
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

	//imguiの初期化
	if (ImGui::CreateContext() == nullptr) {
		assert(0);
		return false;
	}
	bool blnResult = ImGui_ImplWin32_Init(_hwnd);
	if (!blnResult) {
		assert(0);
		return false;
	}
	blnResult = ImGui_ImplDX12_Init(_dx12->Device(),//DirectX12デバイス
		1,//frames_in_flightと説明にはあるがflightの意味が掴めず(後述)
		DXGI_FORMAT_R8G8B8A8_UNORM,//書き込み先RTVのフォーマット
		_dx12->GetHeapForImgui().Get(),//imgui用デスクリプタヒープ
		_dx12->GetHeapForImgui()->GetCPUDescriptorHandleForHeapStart(),//CPUハンドル
		_dx12->GetHeapForImgui()->GetGPUDescriptorHandleForHeapStart());//GPUハンドル



	_pmdRenderer->Init();
	_actor.reset(new PMDActor(_dx12,"Model/初音ミクmetal.pmd"));
	_actor->LoadVMDData("motion/yagokoro.vmd");
	_actor->Move(-10, 0, 10);
	_pmdRenderer->AddActor(_actor);

	auto miku=make_shared<PMDActor>(_dx12, "Model/初音ミク.pmd");
	miku->LoadVMDData("motion/yagokoro.vmd");
	miku->Move(0, 0, 0);
	_pmdRenderer->AddActor(miku);

	auto kaito = make_shared<PMDActor>(_dx12, "Model/カイト.pmd");
	kaito->LoadVMDData("motion/yagokoro.vmd");
	kaito->Move(-5, 0, 5);
	_pmdRenderer->AddActor(kaito);

	auto ruka = make_shared<PMDActor>(_dx12, "Model/巡音ルカ.pmd");
	ruka->LoadVMDData("motion/yagokoro.vmd");
	ruka->Move(10, 0, 10);
	_pmdRenderer->AddActor(ruka);

	auto meiko = make_shared<PMDActor>(_dx12, "Model/咲音メイコ.pmd");
	meiko->LoadVMDData("motion/yagokoro.vmd");
	meiko->Move(-10, 0, 0);
	_pmdRenderer->AddActor(meiko);
	
	auto rin = make_shared<PMDActor>(_dx12, "Model/鏡音リン.pmd");
	rin->LoadVMDData("motion/yagokoro.vmd");
	rin->Move(10, 0, 0);
	_pmdRenderer->AddActor(rin);


	_pmdRenderer->AnimationStart();
	return true;
}
///アプリケーション起動
void 
Application::Run() {
	
	ShowWindow(_hwnd, SW_SHOW);
	MSG msg = {};
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
		//_dx12->DrawPrimitiveShapes();
		_pmdRenderer->BeforeDraw();
		_dx12->DrawToPera1(_pmdRenderer);
		_pmdRenderer->Draw();


		_dx12->DrawAmbientOcculusion();

		////ブルーム用
		//_dx12->DrawShrinkTextureForBlur();

		//2枚目(ペラポリ1→ペラポリ2へ)
		//_dx12->DrawToPera2();

		//3枚目(ペラポリ2→バックバッファへ)
		_dx12->Clear();
		_dx12->Draw(_pmdRenderer);

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		

		ImGui::Begin("Rendering Test Menu");
		ImGui::SetWindowSize(ImVec2(400, 500), ImGuiCond_::ImGuiCond_FirstUseEver);
		static bool blnDebugDisp = false;
		ImGui::Checkbox("Debug Display",&blnDebugDisp);
		static bool blnSSAO = false;
		ImGui::Checkbox("SSAO on/off", &blnSSAO);
		static bool blnShadowmap = false;
		ImGui::Checkbox("Self Shadow on/off", &blnShadowmap);
		constexpr float pi = 3.141592653589f;
		if(ImGui::SliderFloat("Field of view", &fov, pi/6.0f, pi*5.0f/6.0f)){
			_dx12->SetFov(fov);
		}
		static float lightVec[3] = { 1.0,-1.0,1.0f };
		if (ImGui::SliderFloat3("Light vector", lightVec, -1.0f, 1.0f)) {
			_dx12->SetLightVector(lightVec);
		}
		static float bgCol[4] = {0.5f,0.5f,0.5f,1.0f};
		ImGui::ColorPicker4("BG color", bgCol, ImGuiColorEditFlags_::ImGuiColorEditFlags_PickerHueWheel |
														ImGuiColorEditFlags_::ImGuiColorEditFlags_AlphaBar);
		static float bloomCol[3] = {};
		ImGui::ColorPicker3("Bloom color",bloomCol);

		//Dx12Wrapperに対して設定を渡す
		_dx12->SetDebugDisplay(blnDebugDisp);
		_dx12->SetSSAO(blnSSAO);
		_dx12->SetSelfShadow(blnShadowmap);
		
		_dx12->SetBackColor(bgCol);
		_dx12->SetBloomColor(bloomCol);
		

		ImGui::End();
		ImGui::Render();

		_dx12->CmdList()->SetDescriptorHeaps(1, _dx12->GetHeapForImgui().GetAddressOf());
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