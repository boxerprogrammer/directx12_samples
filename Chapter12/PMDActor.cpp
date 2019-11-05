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
	XMMATRIX LookAtMatrix(const XMVECTOR& lookat, XMFLOAT3& up, XMFLOAT3& right) {
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
	XMMATRIX LookAtMatrix(const XMVECTOR& origin, const XMVECTOR& lookat, XMFLOAT3& up, XMFLOAT3& right) {
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
	_localMat = LookAtMatrix(XMLoadFloat3(&XMFLOAT3(x, y, z)), XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0));
}


void
PMDActor::SolveLookAt(const PMDIK& ik) {
	//この関数に来た時点でノードはひとつしかなく、チェーンに入っているノード番号は
	//IKのルートノードのものなので、このルートノードからターゲットに向かうベクトルを考えればよい
	auto rootNode=_boneNodeAddressArray[ik.nodeIdxes[0]];
	auto targetNode = _boneNodeAddressArray[ik.boneIdx];

	auto opos1 = XMLoadFloat3(&rootNode->startPos);
	auto tpos1 = XMLoadFloat3(&targetNode->startPos);

	auto opos2 = XMVector3TransformCoord( opos1,_boneMatrices[ik.nodeIdxes[0]]);
	auto tpos2 = XMVector3TransformCoord( tpos1, _boneMatrices[ik.boneIdx]);


	auto originVec = XMVectorSubtract(tpos1,opos1);
	auto targetVec = XMVectorSubtract(tpos2,opos2);

	originVec = XMVector3Normalize(originVec);
	targetVec = XMVector3Normalize(targetVec);
	_boneMatrices[ik.nodeIdxes[0]]=LookAtMatrix(originVec, targetVec, XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0));
}

void 
PMDActor::SolveCosineIK(const PMDIK& ik) {
	//「軸」を求める
	//もし真ん中が「ひざ」であった場合には強制的にX軸とする。

	vector<XMVECTOR> positions;
	std::array<float, 2> edgeLens;

	//IKチェーンが逆順なので、逆に並ぶようにしている
	auto& targetNode = _boneNodeTable[_boneNameArray[ik.targetIdx]];
	positions.emplace_back(XMLoadFloat3(&targetNode.startPos));

	for (auto& chainBoneIdx : ik.nodeIdxes) {
		auto& boneNode = _boneNodeTable[_boneNameArray[chainBoneIdx]];
		positions.emplace_back(XMLoadFloat3(&boneNode.startPos));
	}

	reverse(positions.begin(), positions.end());

	//元の長さを測っておく
	edgeLens[0] = XMVector3Length(XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = XMVector3Length(XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	//ターゲットとルートノードを現在の行列で座標変換する
	//※ノードは根っこに向かって数えられるため1番がルートになっている
	auto rootPos = positions[0];

	//根っこ
	positions[0] = XMVector3Transform(positions[0], _boneMatrices[ik.nodeIdxes[1]]);

	//ターゲット
	positions[2] = XMVector3Transform(positions[2], _boneMatrices[ik.targetIdx]);

	//ターゲットとルートのベクトルを作っておく
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
	//注意点…IKチェーンは根っこに向かってから数えられるため1が根っこに近い
	auto mat1 = XMMatrixTranslationFromVector(-positions[0]);
	mat1 *= XMMatrixRotationX(theta1);
	mat1 *= XMMatrixTranslationFromVector(positions[0]);

	auto& pareMat = _boneMatrices[ik.nodeIdxes[1]];

	auto mat2 = XMMatrixTranslationFromVector(-positions[1]);
	mat2 *= XMMatrixRotationX(theta2-XM_PI);
	mat2 *= XMMatrixTranslationFromVector(positions[1]);

	_boneMatrices[ik.nodeIdxes[1]] *= mat1;

	_boneMatrices[ik.nodeIdxes[0]] = mat2 * _boneMatrices[ik.nodeIdxes[1]];// _boneMatrices[ik.nodeIdxes[0]] * mat1;// mat2*mat1;// *_boneMatrices[ik.nodeIdxes[0]];
}

void 
PMDActor::SolveCCDIK(const PMDIK& ik) {
	auto& ikMat = _boneMatrices[ik.boneIdx];
	auto& targetMat = _boneMatrices[ik.targetIdx];
	targetMat *= ikMat;
	for (auto& child : ik.nodeIdxes) {
		_boneMatrices[child] = ikMat;
	}
	auto& boneName= _boneNameArray[ik.targetIdx];
	auto targetBoneNode = _boneNodeAddressArray[ik.targetIdx];
	//XMFLOAT3 ikOriginPos = bone.startPos;//そのIKの元の座標
	//XMFLOAT3 ikTargetPos = bone.startPos + offset;//移動後のIKの座標
	//_ikpos = ikTargetPos;//表示用IK座標に代入

	////まずはIKの間にあるボーンの座標の一時変数配列を作って、値をコピーする
	////理由はIK再帰する毎に、ボーン座標が変更されるからである(元の座標は保持が必要なので一次変数に格納)
	//std::vector<XMFLOAT3> tmpBonePositions(iklist.ikchainLen);
	//for (int i = 0; i < iklist.ikchainLen; ++i) {
	//	tmpBonePositions[i] = mesh.Bones()[iklist.ikboneIndices[i]].headpos;
	//}

	////ボーンの根っこ部分(IKから最も遠いボーン)からIKの元の座標へのベクトルを作っておく(軸作成用)
	//XMFLOAT3 ikOriginRootVec = ikOriginPos - tmpBonePositions[iklist.ikchainLen - 1];

	////ボーンの根っこ部分(IKから最も遠いボーン)から移動後IK座標へのベクトルを作っておく(軸作成用)
	//XMFLOAT3 ikTargetRootVec = ikTargetPos - tmpBonePositions[iklist.ikchainLen - 1];

	//float ikmaxLen = Length(ikOriginRootVec);
	//if (Length(ikTargetRootVec) > ikmaxLen) {
	//	XMVECTOR vec = XMLoadFloat3(&ikTargetRootVec);
	//	vec = XMVector3ClampLength(vec, 0.1, ikmaxLen);
	//	XMStoreFloat3(&ikTargetRootVec, vec);
	//	ikTargetPos = ikTargetRootVec + tmpBonePositions[iklist.ikchainLen - 1];
	//	_ikpos = ikTargetPos;
	//}
	//if (ikOriginRootVec == ikTargetRootVec) {
	//	return;
	//}

	////IK移動前から移動後への回転ベクトルを計算しておく
	//XMMATRIX matIkRot = LookAtMatrix(Normalize(ikOriginRootVec), Normalize(ikTargetRootVec), XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0));

	////ここからがCCD-IKだ
	////ホントはここでサイクリック(繰り返す)んですが、まず一回目のことだけ
	////考えます。
	//for (int c = 0; c < iklist.iterations; ++c) {
	//	//中間ボーン座標補正
	//	for (int i = 0; i < iklist.ikchainLen; ++i) {
	//		int ikboneIdx = iklist.ikboneIndices[i];
	//		PMDMesh::Bone& bone = mesh.Bones()[ikboneIdx];
	//		//補正するたびにコントロールポイントの座標は変わるがそこに対するベクトルを再計算していく。
	//		XMFLOAT3 originVec = ikOriginPos - tmpBonePositions[i];//もとの先っちょIKとさかのぼりノードでベクトル作成
	//		XMFLOAT3 targetVec = ikTargetPos - tmpBonePositions[i];//目標地点とさかのぼりノードでベクトルを作成
	//		if (originVec == targetVec)return;
	//		//それぞれのベクトル長が小さすぎる場合は処理を打ち切る
	//		if (Length(originVec) < 0.0001f || Length(targetVec) < 0.0001f) {
	//			return;
	//		}

	//		//正規化します
	//		originVec = Normalize(originVec);
	//		targetVec = Normalize(targetVec);

	//		//外積から軸を作成します
	//		XMFLOAT3 axis = Normalize(Cross(originVec, targetVec));

	//		//もしひざ系なら、x軸を回転軸とする
	//		if (bone.name.find("ひざ") != std::string::npos) {
	//			axis.x = -1;
	//			axis.y = 0;
	//			axis.z = 0;
	//			//その軸をmatIkRotで回転する。
	//			XMVECTOR tmpvec = XMLoadFloat3(&axis);
	//			tmpvec = XMVector3Transform(tmpvec, matIkRot);
	//			XMStoreFloat3(&axis, tmpvec);
	//		}
	//		else {
	//			if (Length(axis) == 0.0f) {
	//				return;//外積結果が0になってるなら使えません
	//			}
	//		}
	//		//ふたつのベクトルの間の角度を計算(制限角度演算のため)
	//		float angle = XMVector3AngleBetweenNormals(XMLoadFloat3(&originVec), XMLoadFloat3(&targetVec)).m128_f32[0];
	//		angle *= 0.5;//ここ半分にしてるの何故…？

	//		////角度が小さすぎる場合は処理を打ち切る
	//		if (abs(angle) == 0.000f) {
	//			return;
	//		}

	//		////制限角度を計算
	//		float strict = iklist.limitAngle * 4;//制限角度は持ってきた角度の４倍
	//		//それ以上に曲げられないようにしとく
	//		angle = min(angle, strict);
	//		angle = max(angle, -strict);

	//		//ボーン変換行列を計算
	//		XMMATRIX rotMat = XMMatrixRotationAxis(XMLoadFloat3(&axis), angle);

	//		//一時ボーン座標は次のベクトルを作るために必要なので再計算する。
	//		XMFLOAT3& tmpbonePosition = tmpBonePositions[i];
	//		XMMATRIX mat = XMMatrixTranslation(-tmpbonePosition.x, -tmpbonePosition.y, -tmpbonePosition.z)*
	//			rotMat*
	//			XMMatrixTranslation(tmpbonePosition.x, tmpbonePosition.y, tmpbonePosition.z);

	//		//ボーン頂点座標を更新
	//		ikOriginPos = ikOriginPos * mat;
	//		for (int j = 0; j < i; ++j) {
	//			tmpBonePositions[j] = tmpBonePositions[j] * mat;
	//		}

	//		//実際にボーンを回転させるための行列を作る(原点に移動→回転→元の座標)
	//		mat = XMMatrixTranslation(-bone.headpos.x, -bone.headpos.y, -bone.headpos.z)*
	//			rotMat*
	//			XMMatrixTranslation(bone.headpos.x, bone.headpos.y, bone.headpos.z);

	//		//変換行列を計算(オフセットを考慮)
	//		mesh.BoneMatrixes()[ikboneIdx] = mesh.BoneMatrixes()[ikboneIdx] * mat;
	//	}
	//}
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
	//IKでかつ、親の影響を受けないならここで親の行列を乗算しないようにしておく
	if (node->boneType == (uint32_t)BoneType::IK && node->ikParentBone == -1)return;
	if (node->boneType == (uint32_t)::BoneType::IKChild && node->ikParentBone>0) {
		//IK情報を検索し、IKのターゲットボーンであれば親の影響を受けないようにしておく
		auto boneIdx = node->ikParentBone;
		auto ikIt = find_if(_ikData.begin(), _ikData.end(), [boneIdx](const PMDIK& ik) {return ik.boneIdx == boneIdx; });
		if (ikIt != _ikData.end()) {
			if (ikIt->targetIdx == node->boneIdx) {
				return;
			}
		}
	}
	_boneMatrices[node->boneIdx] *= mat;
	for (auto& cnode : node->children) {
		RecursiveMatrixMultipy(cnode, _boneMatrices[node->boneIdx]);
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
		_motiondata[f.boneName].emplace_back(KeyFrame(f.frameNo, XMLoadFloat4(&f.quaternion),f.location,
			XMFLOAT2((float)f.bezier[3]/127.0f,(float)f.bezier[7]/127.0f),
			XMFLOAT2((float)f.bezier[11] / 127.0f, (float)f.bezier[15] / 127.0f)));
		_duration = std::max<unsigned int>(_duration, f.frameNo);
	}

	for (auto& motion : _motiondata) {
		sort(motion.second.begin(),motion.second.end(),
			[](const KeyFrame& lval,const KeyFrame& rval){
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

		auto rit=find_if(keyframes.rbegin(), keyframes.rend(), [frameNo](const KeyFrame& keyframe) {
			return keyframe.frameNo <= frameNo;
		});
		if (rit == keyframes.rend())continue;//合致するものがなければ飛ばす
		XMMATRIX rotation=XMMatrixIdentity();
		XMVECTOR offset = XMLoadFloat3(&rit->offset); 
		auto it = rit.base();
		if (it != keyframes.end()) {
			auto t = static_cast<float>(frameNo - rit->frameNo) / 
					static_cast<float>(it->frameNo - rit->frameNo);
			t = GetYFromXOnBezier(t, it->p1, it->p2, 12);
			rotation = XMMatrixRotationQuaternion(
						XMQuaternionSlerp(rit->quaternion,it->quaternion,t)
					);
			offset = XMVectorLerp(offset, XMLoadFloat3(&it->offset), t);
		}
		else {
			rotation=XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)*//原点に戻し
			rotation*//回転
			XMMatrixTranslation(pos.x, pos.y, pos.z);//元の座標に戻す
		_boneMatrices[node.boneIdx] =mat*XMMatrixTranslationFromVector(offset);
	}
	RecursiveMatrixMultipy(&_boneNodeTable["センター"], XMMatrixIdentity());

	IKSolve();

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

void 
PMDActor::IKSolve() {
	//まずはIKのターゲットボーンを動かす
	for (auto& ik : _ikData) {
		auto childrenNodesCount = ik.nodeIdxes.size();
		switch(childrenNodesCount) {
		case 0://間のボーン数が0(ありえない)
			assert(0);
			continue;
		case 1://間のボーン数が1のときはLookAt
			SolveLookAt(ik);
			break;
		case 2://間のボーン数が2のときは余弦定理IK
			SolveCosineIK(ik);
			break;
		case 3://3以上の時はCCD-IK
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
	

	uint16_t ikNum=0;
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
		fread(ik.nodeIdxes.data(), sizeof(ik.nodeIdxes[0]),chainLen, fp);
	}

	fclose(fp);

	//読み込み後の処理

	_boneNameArray.resize(pmdBones.size());
	_boneNodeAddressArray.resize(pmdBones.size());
	//ボーン情報構築
	//インデックスと名前の対応関係構築のために後で使う
	//ボーンノードマップを作る
	for (int idx = 0; idx < pmdBones.size(); ++idx) {
		auto& pb = pmdBones[idx];
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
		node.boneType = pb.type;
		node.ikParentBone = pb.ikBoneNo;
		//インデックス検索がしやすいように
		_boneNameArray[idx] = pb.boneName;
		_boneNodeAddressArray[idx] = &node;
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
			oss << "\tノードボーン=" << node << ":" << getNameFromIdx(node)<<endl;
		}
		OutputDebugString(oss.str().c_str());
	}
}

HRESULT 
PMDActor::CreateTransformView() {
	//GPUバッファ作成
	auto buffSize = sizeof(XMMATRIX)*(1 + _boneMatrices.size());
	buffSize = (buffSize + 0xff)&~0xff;

	auto result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffSize),
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