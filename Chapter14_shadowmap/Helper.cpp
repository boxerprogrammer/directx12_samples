#include<string>
#include<assert.h>
#include "Helper.h"


using namespace std;

Helper::Helper()
{
}


Helper::~Helper()
{
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

std::vector<float> 
GetGaussianValues(float s, size_t sampleNum) {
	std::vector<float> weight(sampleNum);
	float total = 0;//後から割るために合計値を記録
	for (int i = 0; i < sampleNum; ++i) {
		float x = static_cast<float>(i);
		auto wgt= expf(-(x * x) / (2 * s*s));
		weight[i] = wgt;
		total += wgt;
	}
	//ここまでだと、右半分だけなので
	//左半分(左右対称なので、データはいらない)
	//でもトータルは再計算。２倍してデータ0番が
	//重複しているため、これを引く
	total = total * 2 - weight[0];
	for (auto& wgt : weight) {
		wgt /= total;
	}

	return weight;

}