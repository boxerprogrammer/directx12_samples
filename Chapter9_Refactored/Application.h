#pragma once
///シングルトンクラス
class Application
{
private:
	//ここに必要な変数(バッファやヒープなど)を書く

	//↓シングルトンのためにコンストラクタをprivateに
	//さらにコピーと代入を禁止に
	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	///Applicationのシングルトンインスタンスを得る
	static Application& Instance();

	///初期化
	void Init();

	///ループ起動
	void Run();

	///後処理
	void Terminate();

	~Application();
};

