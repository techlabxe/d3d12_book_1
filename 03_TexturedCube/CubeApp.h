#pragma once

#include "../common/D3D12AppBase.h"
#include <DirectXMath.h>

class CubeApp : public D3D12AppBase
{
public:
  CubeApp() : D3D12AppBase() { }

  virtual void Prepare() override;
  virtual void Cleanup() override;
  virtual void MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command) override;

  struct Vertex
  {
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
    DirectX::XMFLOAT2 UV;
  };

  struct ShaderParameters
  {
    DirectX::XMFLOAT4X4 mtxWorld;
    DirectX::XMFLOAT4X4 mtxView;
    DirectX::XMFLOAT4X4 mtxProj;
  };

  enum
  {
    TextureSrvDescriptorBase = 0,
    ConstantBufferDescriptorBase = 1,
    // サンプラーは別ヒープなので先頭を使用
    SamplerDescriptorBase = 0,
  };
private:
  ComPtr<ID3D12Resource1> CreateBuffer(UINT bufferSize, const void* initialData);
  ComPtr<ID3D12Resource1> CreateTexture(const std::string& fileName);
  void PrepareDescriptorHeapForCubeApp();

  ComPtr<ID3D12DescriptorHeap> m_heapSrvCbv;
  ComPtr<ID3D12DescriptorHeap> m_heapSampler;
  UINT  m_samplerDescriptorSize;

  ComPtr<ID3D12Resource1> m_vertexBuffer;
  ComPtr<ID3D12Resource1> m_indexBuffer;
  ComPtr<ID3D12Resource1> m_texture;
  D3D12_VERTEX_BUFFER_VIEW  m_vertexBufferView;
  D3D12_INDEX_BUFFER_VIEW   m_indexBufferView;
  UINT  m_indexCount;

  ComPtr<ID3DBlob>  m_vs, m_ps;
  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12PipelineState> m_pipeline;
  std::vector<ComPtr<ID3D12Resource1>> m_constantBuffers;

  D3D12_GPU_DESCRIPTOR_HANDLE m_sampler;
  D3D12_GPU_DESCRIPTOR_HANDLE m_srv;
  std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_cbViews;
};