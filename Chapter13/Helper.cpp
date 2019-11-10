#include<string>
#include<assert.h>
#include<vector>
#include<cmath>
#include "Helper.h"


using namespace std;

Helper::Helper()
{
}


Helper::~Helper()
{
}

std::vector<float> 
GetGaussianWeights(size_t count, float s) {
	std::vector<float> weights(count);//ウェイト配列返却用
	float x = 0.0f;
	float total = 0.0f;
	for (auto& wgt : weights) {
		wgt = expf(-(x*x) / (2 * s*s));
		total += wgt;
		x += 1.0f;
	}
	//真ん中を中心に左右に広がるように作りますので
	//左右という事で2倍します。しかしその場合は中心の0のピクセルが
	//重複してしまいますのでe^0=1ということで最後に1を引いて辻褄が合うようにしています。
	total = total * 2.0f - 1.0f;
	//足して１になるようにする
	for (auto& wgt : weights) {
		wgt /= total;
	}
	return weights;
}

//１バイトstringをワイド文字wstringに変換する
wstring WStringFromString(const std::string& str) {
	wstring wstr;
	auto wcharNum = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), nullptr, 0);
	wstr.resize(wcharNum);
	wcharNum = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(),
		&wstr[0], wstr.size());
	return wstr;
}

///拡張子を返す
///@param path 元のパス文字列
///@return 拡張子文字列
wstring GetExtension(const wstring& path) {
	int index = path.find_last_of(L'.');
	return path.substr(index + 1, path.length() - index);
}

bool CheckResult(HRESULT &result, ID3DBlob * errBlob)
{
	if (FAILED(result)) {
#ifdef _DEBUG
		if (errBlob!=nullptr) {
			std::string outmsg;
			outmsg.resize(errBlob->GetBufferSize());
			std::copy_n(static_cast<char*>(errBlob->GetBufferPointer()),
				errBlob->GetBufferSize(),
				outmsg.begin());
			OutputDebugString(outmsg.c_str());//出力ウィンドウに出力してね
		}
		assert(SUCCEEDED(result));
#endif
		return false;
	}
	else {
		return true;
	}
}

unsigned int
AligmentedValue(unsigned int size, unsigned int alignment) {
	return (size + alignment - (size%alignment));
}