#include "PMDActor.h"
#include"PMDRenderer.h"
#include"Dx12Wrapper.h"
#include<d3dx12.h>
#include<sstream>
#include <algorithm>
using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

#pragma comment(lib,"winmm.lib")

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

float 
PMDActor::GetYFromXOnBezier(float x, const XMFLOAT2& a, const XMFLOAT2& b, uint8_t n) {
	if (a.x == a.y&&b.x == b.y)return x;//計算不要
	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x;//t^3の係数
	const float k1 = 3 * b.x - 6 * a.x;//t^2の係数
	const float k2 = 3 * a.x;//tの係数

	//誤差の範囲内かどうかに使用する定数
	constexpr float epsilon = 0.0005f;

	for (int i = 0; i < n; ++i) {
		//f(t)求めまーす
		auto ft = k0 * t*t*t + k1 * t*t + k2 * t - x;
		//もし結果が0に近い(誤差の範囲内)なら打ち切り
		if (ft <= epsilon && ft >= -epsilon)break;

		t -= ft / 2;
	}
	//既に求めたいtは求めているのでyを計算する
	auto r = 1 - t;
	return t * t*t + 3 * t*t*r*b.y + 3 * t*r*r*a.y;
}

void* 
PMDActor::Transform::operator new(size_t size) {
	return _aligned_malloc(size, 16);
}

void
PMDActor::RecursiveMatrixMultipy(BoneNode* node, const DirectX::XMMATRIX& mat) {
	_boneMatrices[node->boneIdx] = mat;
	for (auto& cnode : node->children) {
		RecursiveMatrixMultipy(cnode, _boneMatrices[cnode->boneIdx] * mat);
	}
}


PMDActor::PMDActor(const char* filepath,PMDRenderer& renderer):
	_renderer(renderer),
	_dx12(renderer._dx12),
	_angle(0.0f)
{
	_transform.world = XMMatrixIdentity();
	LoadPMDFile(filepath);
	CreateTransformView();
	CreateMaterialData();
	CreateMaterialAndTextureView();



	//RecursiveMatrixMultipy(&_boneNodeTable["センター"], XMMatrixIdentity());
	//XMMatrixRotationQuaternion()
	//copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}


PMDActor::~PMDActor()
{
}

void 
PMDActor::LoadVMDFile(const char* filepath, const char* name) {
	auto fp = fopen(filepath, "rb");
	fseek(fp, 50, SEEK_SET);//最初の50バイトは飛ばしてOK
	unsigned int keyframeNum = 0;
	fread(&keyframeNum, sizeof(keyframeNum), 1, fp);

	struct VMDKeyFrame {
		char boneName[15]; // ボーン名
		unsigned int frameNo; // フレーム番号(読込時は現在のフレーム位置を0とした相対位置)
		XMFLOAT3 location; // 位置
		XMFLOAT4 quaternion; // Quaternion // 回転
		unsigned char bezier[64]; // [4][4][4]  ベジェ補完パラメータ
	};
	vector<VMDKeyFrame> keyframes(keyframeNum);
	for (auto& keyframe : keyframes) {
		fread(keyframe.boneName, sizeof(keyframe.boneName), 1, fp);//ボーン名
		fread(&keyframe.frameNo, sizeof(keyframe.frameNo) +//フレーム番号
			sizeof(keyframe.location) +//位置(IKのときに使用予定)
			sizeof(keyframe.quaternion) +//クオータニオン
			sizeof(keyframe.bezier), 1, fp);//補間ベジェデータ
	}

	//VMDのキーフレームデータから、実際に使用するキーフレームテーブルへ変換
	for (auto& f : keyframes) {
		_motiondata[f.boneName].emplace_back(
			KeyFrame(
				f.frameNo, 
				XMLoadFloat4(&f.quaternion),
				XMFLOAT2((float)f.bezier[3]/127.0f,(float)f.bezier[7]/127.0f),
				XMFLOAT2((float)f.bezier[11] / 127.0f, (float)f.bezier[15] / 127.0f)
			));
	}

	for (auto& motion : _motiondata) {
		sort(motion.second.begin(),motion.second.end(),
			[](const KeyFrame& lval,const KeyFrame& rval){
				return lval.frameNo <= rval.frameNo;
			});
	}

	for (auto& bonemotion : _motiondata) {
		auto node = _boneNodeTable[bonemotion.first];
		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)*
			XMMatrixRotationQuaternion(bonemotion.second[0].quaternion)*
			XMMatrixTranslation(pos.x, pos.y, pos.z);
		_boneMatrices[node.boneIdx] = mat;
	}
	RecursiveMatrixMultipy(&_boneNodeTable["センター"], XMMatrixIdentity());
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);

}

void 
PMDActor::PlayAnimation() {
	_startTime = timeGetTime();
}
void 
PMDActor::MotionUpdate() {

	auto elapsedTime = timeGetTime() - _startTime;//経過時間を測る
	unsigned int frameNo = 30 * (elapsedTime / 1000.0f);


	//行列情報クリア(してないと前フレームのポーズが重ね掛けされてモデルが壊れる)
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	//モーションデータ更新
	for (auto& bonemotion : _motiondata) {
		auto node = _boneNodeTable[bonemotion.first];
		//合致するものを探す
		auto keyframes = bonemotion.second;

		auto rit=find_if(keyframes.rbegin(), keyframes.rend(), [frameNo](const KeyFrame& keyframe) {
			return keyframe.frameNo <= frameNo;
		});
		if (rit == keyframes.rend())continue;//合致するものがなければ飛ばす
		XMMATRIX rotation;
		auto it = rit.base();
		if (it != keyframes.end()) {
		auto t = static_cast<float>(frameNo - rit->frameNo) / 
				static_cast<float>(it->frameNo - rit->frameNo);
		t = GetYFromXOnBezier(t, it->p1, it->p2, 12);

			rotation = XMMatrixRotationQuaternion(
						XMQuaternionSlerp(rit->quaternion,it->quaternion,t)
					);
		}
		else {
			rotation=XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)* //原点に戻し
			rotation* //回転
			XMMatrixTranslation(pos.x, pos.y, pos.z);//元の座標に戻す
		_boneMatrices[node.boneIdx] = mat;
	}
	RecursiveMatrixMultipy(&_boneNodeTable["センター"], XMMatrixIdentity());
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
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

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size());
	//UPLOAD(確保は可能)
	auto result = _dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
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

	heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resDesc = CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0]));
	//設定は、バッファのサイズ以外頂点バッファの設定を使いまわして
	//OKだと思います。
	result = _dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
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

	unsigned short boneNum = 0;
	fread(&boneNum, sizeof(boneNum), 1, fp);
#pragma pack(1)
	//読み込み用ボーン構造体
	struct Bone {
		char boneName[20];//ボーン名
		unsigned short parentNo;//親ボーン番号
		unsigned short nextNo;//先端のボーン番号
		unsigned char type;//ボーン種別
		unsigned short ikBoneNo;//IKボーン番号
		XMFLOAT3 pos;//ボーンの基準点座標
	};
#pragma pack()
	vector<Bone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(Bone), boneNum, fp);
	fclose(fp);

	//インデックスと名前の対応関係構築のために後で使う
	vector<string> boneNames(pmdBones.size());
	//ボーンノードマップを作る
	for (int idx = 0; idx < pmdBones.size(); ++idx) {
		auto& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
	}
	//親子関係を構築する
	for (auto& pb : pmdBones) {
		//親インデックスをチェック(あり得ない番号なら飛ばす)
		if (pb.parentNo >= pmdBones.size()) {
			continue;
		}
		auto parentName = boneNames[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}
	_boneMatrices.resize(pmdBones.size());
	
	//ボーンをすべて初期化する。
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());
}

HRESULT 
PMDActor::CreateTransformView() {
	//GPUバッファ作成
	auto buffSize = sizeof(XMMATRIX)*(1 + _boneMatrices.size());
	buffSize = (buffSize + 0xff)&~0xff;
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(buffSize);

	auto result = _dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	//マップとコピー
	result = _transformBuff->Map(0, nullptr, (void**)&_mappedMatrices);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	_mappedMatrices[0] = _transform.world;
	std::copy(_boneMatrices.begin() ,_boneMatrices.end(),_mappedMatrices+1);

	//ビューの作成
	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.NumDescriptors = 1;//とりあえずワールドひとつ
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;

	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//デスクリプタヒープ種別
	result = _dx12.Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));//生成
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;
	_dx12.Device()->CreateConstantBufferView(&cbvDesc, _transformHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

HRESULT
PMDActor::CreateMaterialData() {
	//マテリアルバッファを作成
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff)&~0xff;
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * _materials.size());//勿体ないけど仕方ないですね
	auto result = _dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
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
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff)&~0xff;
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize;
	
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;//後述
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;//ミップマップは使用しないので1
	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(_materialHeap->GetCPUDescriptorHandleForHeapStart());
	auto incSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (int i = 0; i < _materials.size(); ++i) {
		//マテリアル固定バッファビュー
		_dx12.Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;
		if (_textureResources[i] == nullptr) {
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = _textureResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_textureResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.Offset(incSize);

		if (_sphResources[i] == nullptr) {
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = _sphResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_sphResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		if (_spaResources[i] == nullptr) {
			srvDesc.Format = _renderer._blackTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._blackTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = _spaResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_spaResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;


		if (_toonResources[i] == nullptr) {
			srvDesc.Format = _renderer._gradTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._gradTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = _toonResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_toonResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;
	}
}


void 
PMDActor::Update() {
	//_angle += 0.001f;
	_mappedMatrices[0] =  XMMatrixRotationY(_angle);
	MotionUpdate();
}
void 
PMDActor::Draw() {
	_dx12.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	_dx12.CommandList()->IASetIndexBuffer(&_ibView);

	ID3D12DescriptorHeap* transheaps[] = {_transformHeap.Get()};
	_dx12.CommandList()->SetDescriptorHeaps(1, transheaps);
	_dx12.CommandList()->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());



	ID3D12DescriptorHeap* mdh[] = { _materialHeap.Get() };
	//マテリアル
	_dx12.CommandList()->SetDescriptorHeaps(1, mdh);

	auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	auto cbvsrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
	for (auto& m : _materials) {
		_dx12.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
		_dx12.CommandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}

}