#pragma once

#include "../common/D3D12AppBase.h"
#include <DirectXMath.h>
#include <GLTFSDK/GLTF.h>

namespace Microsoft
{
  namespace glTF
  {
    class Document;
    class GLTFResourceReader;
  }
}

class ModelApp : public D3D12AppBase
{
public:
  ModelApp() : D3D12AppBase() { }

  virtual void Prepare() override;
  virtual void Cleanup() override;
  virtual void MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command) override;

  struct Vertex
  {
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
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
    ConstantBufferDescriptorBase = 0,
    // サンプラーは別ヒープなので先頭を使用
    SamplerDescriptorBase = 0,
  };
private:
  struct BufferObject
  {
    ComPtr<ID3D12Resource1> buffer;
    union
    {
      D3D12_VERTEX_BUFFER_VIEW vertexView;
      D3D12_INDEX_BUFFER_VIEW  indexView;
    };
  };
  struct TextureObject
  {
    ComPtr<ID3D12Resource1> texture;
    DXGI_FORMAT format;
  };
  struct ModelMesh
  {
    BufferObject vertexBuffer;
    BufferObject indexBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;

    int materialIndex;
    
  };
  struct Material
  {
    ComPtr<ID3D12Resource1> texture;
    CD3DX12_GPU_DESCRIPTOR_HANDLE shaderResourceView;
    Microsoft::glTF::AlphaMode alphaMode;
  };
  struct Model
  {
    std::vector<ModelMesh> meshes;
    std::vector<Material>  materials;
  };
  void WaitGPU();

  ComPtr<ID3D12Resource1> CreateBuffer(UINT bufferSize, const void* initialData);
  TextureObject CreateTextureFromMemory(const std::vector<char>& imageData);
  void PrepareDescriptorHeapForModelApp(UINT materialCount);
  void MakeModelGeometry(const Microsoft::glTF::Document& doc, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader);
  void MakeModelMaterial(const Microsoft::glTF::Document& doc, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader);
  ComPtr<ID3D12PipelineState> CreateOpaquePSO();
  ComPtr<ID3D12PipelineState> CreateAlphaPSO();

  ComPtr<ID3D12DescriptorHeap> m_heapSrvCbv;
  ComPtr<ID3D12DescriptorHeap> m_heapSampler;
  UINT  m_samplerDescriptorSize;
  UINT  m_srvDescriptorBase;

  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12PipelineState> m_pipelineOpaque, m_pipelineAlpha;
  std::vector<ComPtr<ID3D12Resource1>> m_constantBuffers;

  D3D12_GPU_DESCRIPTOR_HANDLE m_sampler;
  std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_cbViews;

  Model m_model;

  ComPtr<ID3DBlob> m_vs;
  ComPtr<ID3DBlob> m_psOpaque, m_psAlpha;
};