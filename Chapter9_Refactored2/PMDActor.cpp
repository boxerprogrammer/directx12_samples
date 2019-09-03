#include "PMDActor.h"
#include"Dx12Wrapper.h"
#include<d3dx12.h>
using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

namespace {
	///テクスチャのパスをセパレータ文字で分離する
	///@param path 対象のパス文字列
	///@param splitter 区切り文字
	///@return 分離前後の文字列ペア
	pair<string, string>
		SplitFileName(const std::string& path, const char splitter = '*') {
		int idx = path.find(splitter);
		pair<string, string> ret;
		ret.first = path.substr(0, idx);
		ret.second = path.substr(idx + 1, path.length() - idx - 1);
		return ret;
	}
	///ファイル名から拡張子を取得する
	///@param path 対象のパス文字列
	///@return 拡張子
	string
		GetExtension(const std::string& path) {
		int idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}
	///モデルのパスとテクスチャのパスから合成パスを得る
	///@param modelPath アプリケーションから見たpmdモデルのパス
	///@param texPath PMDモデルから見たテクスチャのパス
	///@return アプリケーションから見たテクスチャのパス
	std::string GetTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath) {
		//ファイルのフォルダ区切りは\と/の二種類が使用される可能性があり
		//ともかく末尾の\か/を得られればいいので、双方のrfindをとり比較する
		//int型に代入しているのは見つからなかった場合はrfindがepos(-1→0xffffffff)を返すため
		int pathIndex1 = modelPath.rfind('/');
		int pathIndex2 = modelPath.rfind('\\');
		auto pathIndex = max(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, pathIndex + 1);
		return folderPath + texPath;
	}
}

PMDActor::PMDActor(const char* filepath,Dx12Wrapper& dx12):_dx12(dx12)
{
}


PMDActor::~PMDActor()
{
}


HRESULT
PMDActor::LoadPMDFile(const char* path) {
	//PMDヘッダ構造体
	struct PMDHeader {
		float version; //例：00 00 80 3F == 1.00
		char model_name[20];//モデル名
		char comment[256];//モデルコメント
	};
	char signature[3];
	PMDHeader pmdheader = {};

	string strModelPath = path;

	auto fp = fopen(strModelPath.c_str(), "rb");
	if (fp == nullptr) {
		//エラー処理
		assert(0);
		return ERROR_FILE_NOT_FOUND;
	}
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	unsigned int vertNum;//頂点数
	fread(&vertNum, sizeof(vertNum), 1, fp);


#pragma pack(1)//ここから1バイトパッキング…アライメントは発生しない
	//PMDマテリアル構造体
	struct PMDMaterial {
		XMFLOAT3 diffuse; //ディフューズ色
		float alpha; // ディフューズα
		float specularity;//スペキュラの強さ(乗算値)
		XMFLOAT3 specular; //スペキュラ色
		XMFLOAT3 ambient; //アンビエント色
		unsigned char toonIdx; //トゥーン番号(後述)
		unsigned char edgeFlg;//マテリアル毎の輪郭線フラグ
		//2バイトのパディングが発生！！
		unsigned int indicesNum; //このマテリアルが割り当たるインデックス数
		char texFilePath[20]; //テクスチャファイル名(プラスアルファ…後述)
	};//70バイトのはず…でもパディングが発生するため72バイト
#pragma pack()//1バイトパッキング解除

	constexpr unsigned int pmdvertex_size = 38;//頂点1つあたりのサイズ
	std::vector<unsigned char> vertices(vertNum*pmdvertex_size);//バッファ確保
	fread(vertices.data(), vertices.size(), 1, fp);//一気に読み込み

	unsigned int indicesNum;//インデックス数
	fread(&indicesNum, sizeof(indicesNum), 1, fp);//

	//UPLOAD(確保は可能)
	auto result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertices.size()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vb.ReleaseAndGetAddressOf()));

	unsigned char* vertMap = nullptr;
	result = _vb->Map(0, nullptr, (void**)&vertMap);
	std::copy(vertices.begin(), vertices.end(), vertMap);
	_vb->Unmap(0, nullptr);


	_vbView.BufferLocation = _vb->GetGPUVirtualAddress();//バッファの仮想アドレス
	_vbView.SizeInBytes = vertices.size();//全バイト数
	_vbView.StrideInBytes = pmdvertex_size;//1頂点あたりのバイト数

	std::vector<unsigned short> indices(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);//一気に読み込み


	//設定は、バッファのサイズ以外頂点バッファの設定を使いまわして
	//OKだと思います。
	result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0])),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_ib.ReleaseAndGetAddressOf()));

	//作ったバッファにインデックスデータをコピー
	unsigned short* mappedIdx = nullptr;
	_ib->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(indices.begin(), indices.end(), mappedIdx);
	_ib->Unmap(0, nullptr);


	//インデックスバッファビューを作成
	_ibView.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = indices.size() * sizeof(indices[0]);

	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);
	_materials.resize(materialNum);
	_textureResources.resize(materialNum);
	_sphResources.resize(materialNum);
	_spaResources.resize(materialNum);
	_toonResources.resize(materialNum);

	std::vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);
	//コピー
	for (int i = 0; i < pmdMaterials.size(); ++i) {
		_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		_materials[i].material.alpha = pmdMaterials[i].alpha;
		_materials[i].material.specular = pmdMaterials[i].specular;
		_materials[i].material.specularity = pmdMaterials[i].specularity;
		_materials[i].material.ambient = pmdMaterials[i].ambient;
		_materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;
	}

	for (int i = 0; i < pmdMaterials.size(); ++i) {
		//トゥーンリソースの読み込み
		char toonFilePath[32];
		sprintf(toonFilePath, "toon/toon%02d.bmp", pmdMaterials[i].toonIdx + 1);
		_toonResources[i] = _dx12.GetTextureByPath(toonFilePath);

		if (strlen(pmdMaterials[i].texFilePath) == 0) {
			_textureResources[i] = nullptr;
			continue;
		}

		string texFileName = pmdMaterials[i].texFilePath;
		string sphFileName = "";
		string spaFileName = "";
		if (count(texFileName.begin(), texFileName.end(), '*') > 0) {//スプリッタがある
			auto namepair = SplitFileName(texFileName);
			if (GetExtension(namepair.first) == "sph") {
				texFileName = namepair.second;
				sphFileName = namepair.first;
			}
			else if (GetExtension(namepair.first) == "spa") {
				texFileName = namepair.second;
				spaFileName = namepair.first;
			}
			else {
				texFileName = namepair.first;
				if (GetExtension(namepair.second) == "sph") {
					sphFileName = namepair.second;
				}
				else if (GetExtension(namepair.second) == "spa") {
					spaFileName = namepair.second;
				}
			}
		}
		else {
			if (GetExtension(pmdMaterials[i].texFilePath) == "sph") {
				sphFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else if (GetExtension(pmdMaterials[i].texFilePath) == "spa") {
				spaFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else {
				texFileName = pmdMaterials[i].texFilePath;
			}
		}
		//モデルとテクスチャパスからアプリケーションからのテクスチャパスを得る
		if (texFileName != "") {
			auto texFilePath = GetTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
			_textureResources[i] = _dx12.GetTextureByPath(texFilePath.c_str());
		}
		if (sphFileName != "") {
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			_sphResources[i] = _dx12.GetTextureByPath(sphFilePath.c_str());
		}
		if (spaFileName != "") {
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			_spaResources[i] = _dx12.GetTextureByPath(spaFilePath.c_str());
		}
	}
	fclose(fp);
}

HRESULT
PMDActor::CreateMaterialData() {
	//マテリアルバッファを作成
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff)&~0xff;
	auto result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize*_materials.size()),//勿体ないけど仕方ないですね
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	//マップマテリアルにコピー
	char* mapMaterial = nullptr;
	result = _materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	for (auto& m : _materials) {
		*((MaterialForHlsl*)mapMaterial) = m.material;//データコピー
		mapMaterial += materialBuffSize;//次のアライメント位置まで進める
	}
	_materialBuff->Unmap(0, nullptr);

	return S_OK;

}


HRESULT 
PMDActor::CreateMaterialAndTextureView() {
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.NumDescriptors = _materials.size() * 5;//マテリアル数ぶん(定数1つ、テクスチャ3つ)
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;

	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//デスクリプタヒープ種別
	auto result = _dx12.Device()->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(_materialHeap.ReleaseAndGetAddressOf()));//生成
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	auto materialBuffSize = _materialBuff->GetDesc().Width;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;//後述
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;//ミップマップは使用しないので1
	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(_materialHeap->GetCPUDescriptorHandleForHeapStart());
	auto incSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (int i = 0; i < _materialNum; ++i) {
		//マテリアル固定バッファビュー
		_dev->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;
		if (_textureResources[i] == nullptr) {
			srvDesc.Format = _whiteTex->GetDesc().Format;
			_dev->CreateShaderResourceView(_whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = _textureResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(_textureResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.Offset(incSize);

		if (_sphResources[i] == nullptr) {
			srvDesc.Format = _whiteTex->GetDesc().Format;
			_dev->CreateShaderResourceView(_whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = _sphResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(_sphResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		if (_spaResources[i] == nullptr) {
			srvDesc.Format = _blackTex->GetDesc().Format;
			_dev->CreateShaderResourceView(_blackTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = _spaResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(_spaResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;


		if (_toonResources[i] == nullptr) {
			srvDesc.Format = _gradTex->GetDesc().Format;
			_dev->CreateShaderResourceView(_gradTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = _toonResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(_toonResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;
	}
}