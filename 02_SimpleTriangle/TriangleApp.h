#pragma once

#include "../common/D3D12AppBase.h"
#include <DirectXMath.h>

class TriangleApp : public D3D12AppBase
{
public:
  TriangleApp() : D3D12AppBase() { }

  virtual void Prepare() override;
  virtual void Cleanup() override;
  virtual void MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command) override;

  struct Vertex
  {
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
  };
private:
  ComPtr<ID3D12Resource1> CreateBuffer(UINT bufferSize, const void* initialData);

  ComPtr<ID3D12Resource1> m_vertexBuffer;
  ComPtr<ID3D12Resource1> m_indexBuffer;
  D3D12_VERTEX_BUFFER_VIEW  m_vertexBufferView;
  D3D12_INDEX_BUFFER_VIEW   m_indexBufferView;
  UINT  m_indexCount;

  ComPtr<ID3DBlob>  m_vs, m_ps;
  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12PipelineState> m_pipeline;

};