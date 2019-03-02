#include "TriangleApp.h"

void TriangleApp::Prepare()
{
  Vertex triangleVertices[] = {
  { {  0.0f, 0.25f, 0.5f }, { 1.0f, 0.0f,0.0f,1.0f} },
  { { 0.25f,-0.25f, 0.5f }, { 0.0f, 1.0f,0.0f,1.0f} },
  { {-0.25f,-0.25f, 0.5f }, { 0.0f, 0.0f,1.0f,1.0f} },
  };
  uint32_t indices[] = { 0, 1, 2 };

  // 頂点バッファとインデックスバッファの生成.
  m_vertexBuffer = CreateBuffer(sizeof(triangleVertices), triangleVertices);
  m_indexBuffer = CreateBuffer(sizeof(indices), indices);
  m_indexCount = _countof(indices);

  // 各バッファのビューを生成.
  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes = sizeof(triangleVertices);
  m_vertexBufferView.StrideInBytes = sizeof(Vertex);
  m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
  m_indexBufferView.SizeInBytes = sizeof(indices);
  m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

  // シェーダーをコンパイル.
  HRESULT hr;
  ComPtr<ID3DBlob> errBlob;
  hr = CompileShaderFromFile(L"VertexShader.hlsl", L"vs_6_0", m_vs, errBlob);
  if (FAILED(hr))
  {
    OutputDebugStringA((const char*)errBlob->GetBufferPointer());
  }
  hr = CompileShaderFromFile(L"PixelShader.hlsl", L"ps_6_0", m_ps, errBlob);
  if (FAILED(hr))
  {
    OutputDebugStringA((const char*)errBlob->GetBufferPointer());
  }

  // ルートシグネチャの構築
  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
  rootSigDesc.Init(
    0, nullptr,   //pParameters
    0, nullptr,   //pStaticSamplers
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
  );
  ComPtr<ID3DBlob> signature;
  hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &errBlob);
  if (FAILED(hr))
  {
    throw std::runtime_error("D3D12SerializeRootSignature faild.");
  }
  // RootSignature の生成
  hr = m_device->CreateRootSignature(
    0,
    signature->GetBufferPointer(), signature->GetBufferSize(),
    IID_PPV_ARGS(&m_rootSignature)
  );
  if (FAILED(hr))
  {
    throw std::runtime_error("CrateRootSignature failed.");
  }

  // インプットレイアウト
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, offsetof(Vertex,Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
  };

  // パイプラインステートオブジェクトの生成.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
  // シェーダーのセット
  psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.Get());
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_ps.Get());
  // ブレンドステート設定
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  // ラスタライザーステート
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  // 出力先は1ターゲット
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  // デプスバッファのフォーマットを設定
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  // ルートシグネチャのセット
  psoDesc.pRootSignature = m_rootSignature.Get();
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  // マルチサンプル設定
  psoDesc.SampleDesc = { 1,0 };
  psoDesc.SampleMask = UINT_MAX; // これを忘れると絵が出ない＆警告も出ないので注意.

  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipeline));
  if (FAILED(hr))
  {
    throw std::runtime_error("CreateGraphicsPipelineState failed");
  }

}

void TriangleApp::Cleanup()
{
  auto index = m_swapchain->GetCurrentBackBufferIndex();
  auto fence = m_frameFences[index];
  auto value = ++m_frameFenceValues[index];
  m_commandQueue->Signal(fence.Get(), value);
  fence->SetEventOnCompletion(value, m_fenceWaitEvent);
  WaitForSingleObject(m_fenceWaitEvent, GpuWaitTimeout);
}

void TriangleApp::MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command)
{
  // パイプラインステートのセット
  command->SetPipelineState(m_pipeline.Get());
  // ルートシグネチャのセット
  command->SetGraphicsRootSignature(m_rootSignature.Get());
  // ビューポートとシザーのセット
  command->RSSetViewports(1, &m_viewport);
  command->RSSetScissorRects(1, &m_scissorRect);

  // プリミティブタイプ、頂点・インデックスバッファのセット
  command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  command->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  command->IASetIndexBuffer(&m_indexBufferView);

  // 描画命令の発行
  command->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}

TriangleApp::ComPtr<ID3D12Resource1> TriangleApp::CreateBuffer(UINT bufferSize, const void* initialData)
{
  HRESULT hr;
  ComPtr<ID3D12Resource1> buffer;
  hr = m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&buffer)
  );

  // 初期データの指定があるときにはコピーする
  if (SUCCEEDED(hr) && initialData != nullptr)
  {
    void* mapped;
    CD3DX12_RANGE range(0, 0);
    hr = buffer->Map(0, &range, &mapped);
    if (SUCCEEDED(hr))
    {
      memcpy(mapped, initialData, bufferSize);
      buffer->Unmap(0, nullptr);
    }
  }

  return buffer;
}
