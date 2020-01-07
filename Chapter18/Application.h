#pragma once
#include<Windows.h>
#include<memory>

class Dx12Wrapper;

struct Size {
	int width;
	int height;
	Size() {}
	Size(int w, int h) :width(w), height(h) {}
};

class PMDActor;
class PMDRenderer;

class Application
{
private:
	HWND _hwnd;//まずこのウィンドウを操作するためのハンドルを作りたい
	WNDCLASSEX _wndClass = {};

	std::shared_ptr<Dx12Wrapper> _dx12;
	std::shared_ptr<PMDActor> _actor;
	std::shared_ptr<PMDRenderer> _pmdRenderer;
	//コンストラクタをprivateにしてnewさせないように
	Application();
	//コピー、代入禁止
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	static Application& Instance();
	///アプリケーション初期化
	bool Initialize();
	void SyncronizeEffekseerCamera();
	///アプリケーション起動
	void Run();
	///アプリケーション終了
	void Terminate();

	Size GetWindowSize();

	~Application();
};

