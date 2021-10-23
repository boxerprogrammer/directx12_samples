#pragma once

#include<d3d12.h>
#include<string>

class Helper
{
public:
	Helper();
	~Helper();
};

///リザルトをチェックし、ダメだったらfalseを返す
///@param result DX関数からの戻り値
///@param errBlob エラーがあるならエラーを出力
///@remarks デバッグ時にはerrBlobよりデバッグ出力を行い
///そのままクラッシュする
extern bool CheckResult(HRESULT &result, ID3DBlob* errBlob=nullptr);

///アライメント数値を返す
///@param size アライメント対象のサイズ
///@param alignment アライメントサイズ
///@retval アライメントされてしまったサイズ
extern unsigned int AligmentedValue(unsigned int size, unsigned int alignment = 16);


//１バイトstringをワイド文字wstringに変換する
extern std::wstring WStringFromString(const std::string& str);

///拡張子を返す
///@param path 元のパス文字列
///@return 拡張子文字列
extern std::wstring GetExtension(const std::wstring& path);

///ガウスウェイト値を返す
///@param count 片側いくつのウェイト値を持っておくか
///@param s ガウス関数におけるσ値
///@retval ガウスウェイト配列(countで指定された可変長配列)
///@remarks 全てのウェイト値を足したら1になるはずなので
extern std::vector<float> GetGaussianWeights(size_t count, float s);