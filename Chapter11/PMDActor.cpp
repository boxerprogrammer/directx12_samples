#include "PMDActor.h"
#include"PMDRenderer.h"
#include"Dx12Wrapper.h"
#include<d3dx12.h>
#include<sstream>
#include<array>
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


	///Z軸を特定の方向を向かす行列を返す関数
	///@param lookat 向かせたい方向ベクトル
	///@param up 上ベクトル
	///@param right 右ベクトル
	XMMATRIX LookAtMatrix(const XMVECTOR& lookat, const XMFLOAT3& up, const XMFLOAT3& right) {
		//向かせたい方向(z軸)
		XMVECTOR vz = lookat;

		//(向かせたい方向を向かせたときの)仮のy軸ベクトル
		XMVECTOR vy = XMVector3Normalize(XMLoadFloat3(&up));

		//(向かせたい方向を向かせたときの)y軸
		//XMVECTOR vx = XMVector3Normalize(XMVector3Cross(vz, vx));
		XMVECTOR vx = XMVector3Normalize(XMVector3Cross(vy, vz));
		vy = XMVector3Normalize(XMVector3Cross(vz, vx));

		///LookAtとupが同じ方向を向いてたらright基準で作り直す
		if (abs(XMVector3Dot(vy, vz).m128_f32[0]) == 1.0f) {
			//仮のX方向を定義
			vx = XMVector3Normalize(XMLoadFloat3(&right));
			//向かせたい方向を向かせたときのY軸を計算
			vy = XMVector3Normalize(XMVector3Cross(vz, vx));
			//真のX軸を計算
			vx = XMVector3Normalize(XMVector3Cross(vy, vz));
		}
		XMMATRIX ret = XMMatrixIdentity();
		ret.r[0] = vx;
		ret.r[1] = vy;
		ret.r[2] = vz;
		return ret;
	}

	///特定のベクトルを特定の方向に向けるための行列を返す
	///@param origin 特定のベクトル
	///@param lookat 向かせたい方向
	///@param up 上ベクトル
	///@param right 右ベクトル
	///@retval 特定のベクトルを特定の方向に向けるための行列
	XMMATRIX LookAtMatrix(const XMVECTOR& origin, const XMVECTOR& lookat, const XMFLOAT3& up, const XMFLOAT3& right) {
		return XMMatrixTranspose(LookAtMatrix(origin, up, right))*
			LookAtMatrix(lookat, up, right);
	}
	//ボーン種別
	enum class BoneType {
		Rotation,//回転
		RotAndMove,//回転＆移動
		IK,//IK
		Undefined,//未定義
		IKChild,//IK影響ボーン
		RotationChild,//回転影響ボーン
		IKDestination,//IK接続先
		Invisible//見えないボーン
	};

}

void
PMDActor::LookAt(float x, float y, float z) {
	auto source = XMFLOAT3(x, y, z);
	_localMat = LookAtMatrix(XMLoadFloat3(&source), XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0));
}


void
PMDActor::SolveLookAt(const PMDIK& ik) {
	//この関数に来た時点でノードはひとつしかなく、チェーンに入っているノード番号は
	//IKのルートノードのものなので、このルートノードからターゲットに向かうベクトルを考えればよい
	auto rootNode = _boneNodeAddressArray[ik.nodeIdxes[0]];
	auto targetNode = _boneNodeAddressArray[ik.targetIdx];//!?

	auto opos1 = XMLoadFloat3(&rootNode->startPos);
	auto tpos1 = XMLoadFloat3(&targetNode->startPos);

	auto opos2 = XMVector3Transform(opos1, _boneMatrices[ik.nodeIdxes[0]]);
	auto tpos2 = XMVector3Transform(tpos1, _boneMatrices[ik.boneIdx]);

	auto originVec = XMVectorSubtract(tpos1, opos1);
	auto targetVec = XMVectorSubtract(tpos2, opos2);

	originVec = XMVector3Normalize(originVec);
	targetVec = XMVector3Normalize(targetVec);

	XMMATRIX mat = XMMatrixTranslationFromVector(-opos2)*
					LookAtMatrix(originVec, targetVec, XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0))*
					XMMatrixTranslationFromVector(opos2);

	//auto parent = _boneNodeAddressArray[ik.boneIdx]->parentBone;

	_boneMatrices[ik.nodeIdxes[0]] = mat;// _boneMatrices[ik.boneIdx] * _boneMatrices[parent];
	//_boneMatrices[ik.targetIdx] = _boneMatrices[parent];
}

void
PMDActor::SolveCosineIK(const PMDIK& ik) {
	vector<XMVECTOR> positions;//IK構成点を保存
	std::array<float, 2> edgeLens;//IKのそれぞれのボーン間の距離を保存

	//ターゲット(末端ボーンではなく、末端ボーンが近づく目標ボーンの座標を取得)
	auto& targetNode = _boneNodeAddressArray[ik.boneIdx];
	auto targetPos = XMVector3Transform(XMLoadFloat3(&targetNode->startPos), _boneMatrices[ik.boneIdx]);

	//IKチェーンが逆順なので、逆に並ぶようにしている
	//末端ボーン
	auto endNode = _boneNodeAddressArray[ik.targetIdx];
	positions.emplace_back(XMLoadFloat3(&endNode->startPos));
	//中間及びルートボーン
	for (auto& chainBoneIdx : ik.nodeIdxes) {
		auto boneNode = _boneNodeAddressArray[chainBoneIdx];
		positions.emplace_back(XMLoadFloat3(&boneNode->startPos));
	}
	//ちょっと分かりづらいと思ったので逆にしておきます。そうでもない人はそのまま
	//計算してもらって構わないです。
	reverse(positions.begin(), positions.end());

	//元の長さを測っておく
	edgeLens[0] = XMVector3Length(XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = XMVector3Length(XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	//ルートボーン座標変換(逆順になっているため使用するインデックスに注意)
	positions[0] = XMVector3Transform(positions[0], _boneMatrices[ik.nodeIdxes[1]]);
	//真ん中はどうせ自動計算されるので計算しない
	//先端ボーン
	positions[2] = XMVector3Transform(positions[2], _boneMatrices[ik.boneIdx]);//ホンマはik.targetIdxだが…！？

	//ルートから先端へのベクトルを作っておく
	auto linearVec = XMVectorSubtract(positions[2], positions[0]);
	float A = XMVector3Length(linearVec).m128_f32[0];
	float B = edgeLens[0];
	float C = edgeLens[1];

	linearVec = XMVector3Normalize(linearVec);

	//ルートから真ん中への角度計算
	float theta1 = acosf((A*A + B * B - C * C) / (2 * A*B));

	//真ん中からターゲットへの角度計算
	float theta2 = acosf((B*B + C * C - A * A) / (2 * B*C));

	//「軸」を求める
	//もし真ん中が「ひざ」であった場合には強制的にX軸とする。
	XMVECTOR axis;
	if (find(_kneeIdxes.begin(), _kneeIdxes.end(), ik.nodeIdxes[0]) == _kneeIdxes.end()) {
		auto vm = XMVector3Normalize(XMVectorSubtract(positions[2], positions[0]));
		auto vt = XMVector3Normalize(XMVectorSubtract(targetPos, positions[0]));
		axis = XMVector3Cross(vt, vm);
	}
	else {
		auto right = XMFLOAT3(1, 0, 0);
		axis = XMLoadFloat3(&right);
	}

	//注意点…IKチェーンは根っこに向かってから数えられるため1が根っこに近い
	auto mat1 = XMMatrixTranslationFromVector(-positions[0]);
	mat1 *= XMMatrixRotationAxis(axis,theta1);
	mat1 *= XMMatrixTranslationFromVector(positions[0]);

	
	auto mat2 = XMMatrixTranslationFromVector(-positions[1]);
	mat2 *= XMMatrixRotationAxis(axis,theta2-XM_PI);
	mat2 *= XMMatrixTranslationFromVector(positions[1]);

	_boneMatrices[ik.nodeIdxes[1]] *= mat1;
	_boneMatrices[ik.nodeIdxes[0]] = mat2 * _boneMatrices[ik.nodeIdxes[1]];
	_boneMatrices[ik.targetIdx] = _boneMatrices[ik.nodeIdxes[0]];//直前の影響を受ける
	//_boneMatrices[ik.nodeIdxes[1]] = _boneMatrices[ik.boneIdx];
	//_boneMatrices[ik.nodeIdxes[0]] = _boneMatrices[ik.boneIdx];
	//_boneMatrices[ik.targetIdx] *= _boneMatrices[ik.boneIdx];
}
//誤差の範囲内かどうかに使用する定数
constexpr float epsilon = 0.0005f;
void
PMDActor::SolveCCDIK(const PMDIK& ik) {
	//ターゲット
	auto targetBoneNode = _boneNodeAddressArray[ik.boneIdx];
	auto targetOriginPos = XMLoadFloat3(&targetBoneNode->startPos);

	auto parentMat = _boneMatrices[_boneNodeAddressArray[ik.boneIdx]->ikParentBone];
	XMVECTOR det;
	auto invParentMat = XMMatrixInverse(&det, parentMat);
	auto targetNextPos = XMVector3Transform(targetOriginPos, _boneMatrices[ik.boneIdx] * invParentMat);


	//まずはIKの間にあるボーンの座標を入れておく(逆順注意)
	std::vector<XMVECTOR> bonePositions;
	//auto endPos = XMVector3Transform(
	//	XMLoadFloat3(&_boneNodeAddressArray[ik.targetIdx]->startPos),
	//	//_boneMatrices[ik.targetIdx]);
	//	XMMatrixIdentity());
	//末端ノード
	auto endPos = XMLoadFloat3(&_boneNodeAddressArray[ik.targetIdx]->startPos);
	//中間ノード(ルートを含む)
	for (auto& cidx : ik.nodeIdxes) {
		//bonePositions.emplace_back(XMVector3Transform(XMLoadFloat3(&_boneNodeAddressArray[cidx]->startPos),
			//_boneMatrices[cidx] ));
		bonePositions.push_back(XMLoadFloat3(&_boneNodeAddressArray[cidx]->startPos));
	}

	vector<XMMATRIX> mats(bonePositions.size());
	fill(mats.begin(), mats.end(), XMMatrixIdentity());
	//ちょっとよくわからないが、PMDエディタの6.8°が0.03になっており、これは180で割っただけの値である。
	//つまりこれをラジアンとして使用するにはXM_PIを乗算しなければならない…と思われる。
	auto ikLimit = ik.limit*XM_PI;
	//ikに設定されている試行回数だけ繰り返す
	for (int c = 0; c < ik.iterations; ++c) {
		//ターゲットと末端がほぼ一致したら抜ける
		if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon) {
			break;
		}
		//それぞれのボーンを遡りながら角度制限に引っ掛からないように曲げていく
		for (int bidx = 0; bidx < bonePositions.size(); ++bidx) {
			const auto& pos = bonePositions[bidx];

			//まず現在のノードから末端までと、現在のノードからターゲットまでのベクトルを作る
			auto vecToEnd = XMVectorSubtract(endPos, pos);
			auto vecToTarget = XMVectorSubtract(targetNextPos, pos);
			vecToEnd = XMVector3Normalize(vecToEnd);
			vecToTarget = XMVector3Normalize(vecToTarget);

			//ほぼ同じベクトルになってしまった場合は外積できないため次のボーンに引き渡す
			if (XMVector3Length(XMVectorSubtract(vecToEnd, vecToTarget)).m128_f32[0] <= epsilon) {
				continue;
			}
			//外積計算および角度計算
			auto cross = XMVector3Normalize(XMVector3Cross(vecToEnd, vecToTarget));
			float angle = XMVector3AngleBetweenVectors(vecToEnd, vecToTarget).m128_f32[0];
			angle = min(angle,ikLimit);//回転限界補正
			XMMATRIX rot = XMMatrixRotationAxis(cross, angle);//回転行列
			//posを中心に回転
			auto mat = XMMatrixTranslationFromVector(-pos)*
				rot*
				XMMatrixTranslationFromVector(pos);
			mats[bidx] *= mat;//回転行列を保持しておく(乗算で回転重ね掛けを作っておく)
			//対象となる点をすべて回転させる(現在の点から見て末端側を回転)
			for (auto idx = bidx - 1; idx >= 0; --idx) {//自分を回転させる必要はない
				bonePositions[idx] = XMVector3Transform(bonePositions[idx], mat);
			}
			endPos = XMVector3Transform(endPos, mat);
			//もし正解に近くなってたらループを抜ける
			if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon) {
				break;
			}
		}
	}
	int idx = 0;
	for (auto& cidx : ik.nodeIdxes) {
		_boneMatrices[cidx] = mats[idx];
		++idx;
	}
	auto node = _boneNodeAddressArray[ik.nodeIdxes.back()];
	RecursiveMatrixMultipy(node, parentMat, true);

}

float
PMDActor::GetYFromXOnBezier(float x, const XMFLOAT2& a, const XMFLOAT2& b, uint8_t n) {
	if (a.x == a.y&&b.x == b.y)return x;//計算不要
	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x;//t^3の係数
	const float k1 = 3 * b.x - 6 * a.x;//t^2の係数
	const float k2 = 3 * a.x;//tの係数



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
PMDActor::RecursiveMatrixMultipy(BoneNode* node, const DirectX::XMMATRIX& mat, bool flg) {
	_boneMatrices[node->boneIdx] *= mat;
	for (auto& cnode : node->children) {
		RecursiveMatrixMultipy(cnode, _boneMatrices[node->boneIdx]);
	}
}


PMDActor::PMDActor(const char* filepath, PMDRenderer& renderer) :
	_renderer(renderer),
	_dx12(renderer._dx12),
	_angle(0.0f)
{
	_transform.world = XMMatrixIdentity();
	LoadPMDFile(filepath);
	CreateTransformView();
	CreateMaterialData();
	CreateMaterialAndTextureView();
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

#pragma pack(1)
	//表情データ(頂点モーフデータ)
	struct VMDMorph {
		char name[15];//名前(パディングしてしまう)
		uint32_t frameNo;//フレーム番号
		float weight;//ウェイト(0.0f～1.0f)
	};//全部で23バイトなのでpragmapackで読む
#pragma pack()
	uint32_t morphCount = 0;
	fread(&morphCount, sizeof(morphCount), 1, fp);
	vector<VMDMorph> morphs(morphCount);
	fread(morphs.data(), sizeof(VMDMorph), morphCount, fp);

#pragma pack(1)
	//カメラ
	struct VMDCamera { 
		uint32_t frameNo; // フレーム番号
		float distance; // 距離
		XMFLOAT3 pos; // 座標
		XMFLOAT3 eulerAngle; // オイラー角
		uint8_t Interpolation[24]; // 補完
		uint32_t fov; // 視界角
		uint8_t persFlg; // パースフラグON/OFF
	};//61バイト(これもpragma pack(1)の必要あり)
#pragma pack()
	uint32_t vmdCameraCount = 0;
	fread(&vmdCameraCount, sizeof(vmdCameraCount), 1, fp);
	vector<VMDCamera> cameraData(vmdCameraCount);
	fread(cameraData.data(), sizeof(VMDCamera), vmdCameraCount, fp);

	// ライト照明データ
	struct VMDLight {
		uint32_t frameNo; // フレーム番号
		XMFLOAT3 rgb; //ライト色
		XMFLOAT3 vec; //光線ベクトル(平行光線)
	};

	uint32_t vmdLightCount = 0;
	fread(&vmdLightCount, sizeof(vmdLightCount), 1, fp);
	vector<VMDLight> lights(vmdLightCount);
	fread(lights.data(), sizeof(VMDLight), vmdLightCount, fp);

#pragma pack(1)
	// セルフ影データ
	struct VMDSelfShadow { 
		uint32_t frameNo; // フレーム番号
		uint8_t mode; //影モード(0:影なし、1:モード１、2:モード２)
		float distance; //距離
	};
#pragma pack()
	uint32_t selfShadowCount = 0;
	fread(&selfShadowCount, sizeof(selfShadowCount), 1, fp);
	vector<VMDSelfShadow> selfShadowData(selfShadowCount);
	fread(selfShadowData.data(), sizeof(VMDSelfShadow), selfShadowCount,fp);

	//IKオンオフ切り替わり数
	uint32_t ikSwitchCount=0;
	fread(&ikSwitchCount, sizeof(ikSwitchCount), 1, fp);
	//IK切り替えのデータ構造は少しだけ特殊で、いくつ切り替えようが
	//そのキーフレームは消費されます。その中で切り替える可能性のある
	//IKの名前とそのフラグがすべて登録されている状態です。
	
	//ここからは気を遣って読み込みます。キーフレームごとのデータであり
	//IKボーン(名前で検索)ごとにオン、オフフラグを持っているというデータであるとして
	//構造体を作っていきましょう。
	_ikEnableData.resize(ikSwitchCount);
	for (auto& ikEnable : _ikEnableData) {
		//キーフレーム情報なのでまずはフレーム番号読み込み
		fread(&ikEnable.frameNo, sizeof(ikEnable.frameNo), 1, fp);
		//次に可視フラグがありますがこれは使用しないので1バイトシークでも構いません
		uint8_t visibleFlg = 0;
		fread(&visibleFlg, sizeof(visibleFlg), 1, fp);
		//対象ボーン数読み込み
		uint32_t ikBoneCount = 0;
		fread(&ikBoneCount, sizeof(ikBoneCount), 1, fp);
		//ループしつつ名前とON/OFF情報を取得
		for (int i = 0; i < ikBoneCount; ++i) {
			char ikBoneName[20];
			fread(ikBoneName, _countof(ikBoneName), 1, fp);
			uint8_t flg = 0;
			fread(&flg, sizeof(flg), 1, fp);
			ikEnable.ikEnableTable[ikBoneName] = flg;
		}
	}
	fclose(fp);

	//VMDのキーフレームデータから、実際に使用するキーフレームテーブルへ変換
	for (auto& f : keyframes) {
		_motiondata[f.boneName].emplace_back(KeyFrame(f.frameNo, XMLoadFloat4(&f.quaternion), f.location,
			XMFLOAT2((float)f.bezier[3] / 127.0f, (float)f.bezier[7] / 127.0f),
			XMFLOAT2((float)f.bezier[11] / 127.0f, (float)f.bezier[15] / 127.0f)));
		_duration = std::max<unsigned int>(_duration, f.frameNo);
	}

	for (auto& motion : _motiondata) {
		sort(motion.second.begin(), motion.second.end(),
			[](const KeyFrame& lval, const KeyFrame& rval) {
			return lval.frameNo < rval.frameNo;
		});
	}

	for (auto& bonemotion : _motiondata) {
		auto itBoneNode = _boneNodeTable.find(bonemotion.first);
		if (itBoneNode == _boneNodeTable.end()) {
			continue;
		}
		auto& node = itBoneNode->second;
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
	if (frameNo > _duration) {
		_startTime = timeGetTime();
		frameNo = 0;
	}

	//行列情報クリア(してないと前フレームのポーズが重ね掛けされてモデルが壊れる)
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	//モーションデータ更新
	for (auto& bonemotion : _motiondata) {
		auto& boneName = bonemotion.first;
		auto itBoneNode = _boneNodeTable.find(boneName);
		if (itBoneNode == _boneNodeTable.end()) {
			continue;
		}
		auto node = itBoneNode->second;


		//合致するものを探す
		auto keyframes = bonemotion.second;

		auto rit = find_if(keyframes.rbegin(), keyframes.rend(), [frameNo](const KeyFrame& keyframe) {
			return keyframe.frameNo <= frameNo;
		});
		if (rit == keyframes.rend())continue;//合致するものがなければ飛ばす
		XMMATRIX rotation = XMMatrixIdentity();
		XMVECTOR offset = XMLoadFloat3(&rit->offset);
		auto it = rit.base();
		if (it != keyframes.end()) {
			auto t = static_cast<float>(frameNo - rit->frameNo) /
				static_cast<float>(it->frameNo - rit->frameNo);
			t = GetYFromXOnBezier(t, it->p1, it->p2, 12);
			rotation = XMMatrixRotationQuaternion(
				XMQuaternionSlerp(rit->quaternion, it->quaternion, t)
			);
			offset = XMVectorLerp(offset, XMLoadFloat3(&it->offset), t);
		}
		else {
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)*//原点に戻し
			rotation*//回転
			XMMatrixTranslation(pos.x, pos.y, pos.z);//元の座標に戻す
		_boneMatrices[node.boneIdx] = mat * XMMatrixTranslationFromVector(offset);
	}
	RecursiveMatrixMultipy(&_boneNodeTable["センター"], XMMatrixIdentity());

	IKSolve(frameNo);

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

void
PMDActor::IKSolve(int frameNo) {
	//いつもの逆から検索
	auto it=find_if(_ikEnableData.rbegin(),_ikEnableData.rend(),
		[frameNo](const VMDIKEnable& ikenable) {
			return ikenable.frameNo <= frameNo;
		});
	//まずはIKのターゲットボーンを動かす
	for (auto& ik : _ikData) {//IK解決のためのループ
		if (it != _ikEnableData.rend()) {
			auto ikEnableIt = it->ikEnableTable.find(_boneNameArray[ik.boneIdx]);
			if (ikEnableIt != it->ikEnableTable.end()) {
				if (!ikEnableIt->second) {//もしOFFなら打ち切ります
					continue;
				}
			}
		}
		auto childrenNodesCount = ik.nodeIdxes.size();
		switch (childrenNodesCount) {
		case 0://間のボーン数が0(ありえない)
			assert(0);
			continue;
		case 1://間のボーン数が1のときはLookAt
			SolveLookAt(ik);
			break;
		case 2://間のボーン数が2のときは余弦定理IK
			SolveCosineIK(ik);
			break;
		default://3以上の時はCCD-IK
			SolveCCDIK(ik);
		}
	}
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


	uint16_t ikNum = 0;
	fread(&ikNum, sizeof(ikNum), 1, fp);

	_ikData.resize(ikNum);
	for (auto& ik : _ikData) {
		fread(&ik.boneIdx, sizeof(ik.boneIdx), 1, fp);
		fread(&ik.targetIdx, sizeof(ik.targetIdx), 1, fp);
		uint8_t chainLen = 0;
		fread(&chainLen, sizeof(chainLen), 1, fp);
		ik.nodeIdxes.resize(chainLen);
		fread(&ik.iterations, sizeof(ik.iterations), 1, fp);
		fread(&ik.limit, sizeof(ik.limit), 1, fp);
		if (chainLen == 0)continue;//間ノード数が0ならばここで終わり
		fread(ik.nodeIdxes.data(), sizeof(ik.nodeIdxes[0]), chainLen, fp);
	}

	fclose(fp);

	//読み込み後の処理

	_boneNameArray.resize(pmdBones.size());
	_boneNodeAddressArray.resize(pmdBones.size());
	//ボーン情報構築
	//インデックスと名前の対応関係構築のために後で使う
	//ボーンノードマップを作る
	_kneeIdxes.clear();
	for (int idx = 0; idx < pmdBones.size(); ++idx) {
		auto& pb = pmdBones[idx];
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
		node.boneType = pb.type;
		node.parentBone = pb.parentNo;
		node.ikParentBone = pb.ikBoneNo;
		//インデックス検索がしやすいように
		_boneNameArray[idx] = pb.boneName;
		_boneNodeAddressArray[idx] = &node;
		string boneName = pb.boneName;
		if (boneName.find("ひざ") != std::string::npos) {
			_kneeIdxes.emplace_back(idx);
		}
	}
	//ツリー親子関係を構築する
	for (auto& pb : pmdBones) {
		//親インデックスをチェック(あり得ない番号なら飛ばす)
		if (pb.parentNo >= pmdBones.size()) {
			continue;
		}
		auto parentName = _boneNameArray[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}

	//ボーン構築
	_boneMatrices.resize(pmdBones.size());
	//ボーンをすべて初期化する。
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());



	//IKデバッグ用
	auto getNameFromIdx = [&](uint16_t idx)->string {
		auto it = find_if(_boneNodeTable.begin(), _boneNodeTable.end(), [idx](const pair<string, BoneNode>& obj) {
			return obj.second.boneIdx == idx;
		});
		if (it != _boneNodeTable.end()) {
			return it->first;
		}
		else {
			return "";
		}
	};
	for (auto& ik : _ikData) {
		std::ostringstream oss;
		oss << "IKボーン番号=" << ik.boneIdx << ":" << getNameFromIdx(ik.boneIdx) << endl;
		for (auto& node : ik.nodeIdxes) {
			oss << "\tノードボーン=" << node << ":" << getNameFromIdx(node) << endl;
		}
		OutputDebugString(oss.str().c_str());
	}
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
	std::copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);

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
	_angle += 0.001f;
	_mappedMatrices[0] = XMMatrixRotationY(_angle);
	MotionUpdate();
}
void
PMDActor::Draw() {
	_dx12.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	_dx12.CommandList()->IASetIndexBuffer(&_ibView);

	ID3D12DescriptorHeap* transheaps[] = { _transformHeap.Get() };
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