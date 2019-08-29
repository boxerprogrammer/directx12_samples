#pragma once

#include<d3d12.h>
#include<vector>
#include<wrl.h>
#include<memory>

class PMDActor;
class PMDRenderer
{
	
private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	ComPtr< ID3D12PipelineState> _pipeline = nullptr;//PMD用パイプライン
	ComPtr< ID3D12RootSignature> _rootSignature = nullptr;//PMD用ルートシグネチャ
public:
	PMDRenderer();
	~PMDRenderer();
	void Update();
	void Draw();
};

