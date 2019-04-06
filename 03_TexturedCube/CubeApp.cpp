#include "CubeApp.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../common/stb_image.h"

void CubeApp::Prepare()
{
  const float k = 1.0f;
  const DirectX::XMFLOAT4 red(1.0f, 0.0f, 0.0f,1.0f);
  const DirectX::XMFLOAT4 green(0.0f, 1.0f, 0.0f,1.0f);
  const DirectX::XMFLOAT4 blue(0.0f, 0.0f, 1.0f,1.0);
  const DirectX::XMFLOAT4 white(1.0f, 1.0f,1.0f,1.0f);
  const DirectX::XMFLOAT4 black(0.0f, 0.0f,0.0f,1.0f);
  const DirectX::XMFLOAT4 yellow(1.0f, 1.0f, 0.0f,1.0f);
  const DirectX::XMFLOAT4 magenta(1.0f, 0.0f, 1.0f,1.0f);
  const DirectX::XMFLOAT4 cyan(0.0f, 1.0f, 1.0f,1.0f);
  Vertex triangleVertices[] = {
  // 正面
    { {-k,-k,-k}, red, { 0.0f, 1.0f} },
    { {-k, k,-k}, yellow, { 0.0f, 0.0f} },
    { { k, k,-k}, white, { 1.0f, 0.0f} },
    { { k,-k,-k}, magenta, { 1.0f, 1.0f} },
    // 右
    { { k,-k,-k}, magenta, { 0.0f, 1.0f} },
    { { k, k,-k}, white, { 0.0f, 0.0f} },
    { { k, k, k}, cyan, { 1.0f, 0.0f} },
    { { k,-k, k}, blue, { 1.0f, 1.0f} },
    // 左
    { {-k,-k, k}, black, { 0.0f, 1.0f} },
    { {-k, k, k}, green, { 0.0f, 0.0f} },
    { {-k, k,-k}, yellow, { 1.0f, 0.0f} },
    { {-k,-k,-k}, red, { 1.0f, 1.0f} },
    // 裏
    { { k,-k, k}, blue, { 0.0f, 1.0f} },
    { { k, k, k}, cyan, { 0.0f, 0.0f} },
    { {-k, k, k}, green, { 1.0f, 0.0f} },
    { {-k,-k, k}, black, { 1.0f, 1.0f} },
    // 上
    { {-k, k,-k}, yellow, { 0.0f, 1.0f} },
    { {-k, k, k}, green, { 0.0f, 0.0f} },
    { { k, k, k}, cyan, { 1.0f, 0.0f} },
    { { k, k,-k}, white, { 1.0f, 1.0f} },
    // 底
    { {-k,-k, k}, red, { 0.0f, 1.0f} },
    { {-k,-k,-k}, red, { 0.0f, 0.0f} },
    { { k,-k,-k}, magenta, { 1.0f, 0.0f} },
    { { k,-k, k}, blue, { 1.0f, 1.0f} },
  };
  uint32_t indices[] = { 
    0, 1, 2, 2, 3,0,
    4, 5, 6, 6, 7,4,
    8, 9, 10, 10, 11, 8,
    12,13,14, 14,15,12,
    16,17,18, 18,19,16,
    20,21,22, 22,23,20,
  };

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
  hr = CompileShaderFromFile(L"simpleTexVS.hlsl", L"vs_6_0", m_vs, errBlob);
  if (FAILED(hr))
  {
    OutputDebugStringA((const char*)errBlob->GetBufferPointer());
  }
  hr = CompileShaderFromFile(L"simpleTexPS.hlsl", L"ps_6_0", m_ps, errBlob);
  if (FAILED(hr))
  {
    OutputDebugStringA((const char*)errBlob->GetBufferPointer());
  }

  CD3DX12_DESCRIPTOR_RANGE cbv, srv, sampler;
  cbv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0 レジスタ
  srv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 レジスタ
  sampler.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0); // s0 レジスタ

  CD3DX12_ROOT_PARAMETER rootParams[3];
  rootParams[0].InitAsDescriptorTable(1, &cbv, D3D12_SHADER_VISIBILITY_VERTEX);
  rootParams[1].InitAsDescriptorTable(1, &srv, D3D12_SHADER_VISIBILITY_PIXEL);
  rootParams[2].InitAsDescriptorTable(1, &sampler, D3D12_SHADER_VISIBILITY_PIXEL);

  // ルートシグネチャの構築
  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
  rootSigDesc.Init(
    _countof(rootParams), rootParams,   //pParameters
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
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex,UV), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
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
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

  psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };

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

  PrepareDescriptorHeapForCubeApp();

  // 定数バッファ/定数バッファビューの生成
  m_constantBuffers.resize(FrameBufferCount);
  m_cbViews.resize(FrameBufferCount);
  for (UINT i = 0; i < FrameBufferCount; ++i)
  {
    UINT bufferSize = sizeof(ShaderParameters) + 255 & ~255;
    m_constantBuffers[i] = CreateBuffer(bufferSize, nullptr);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc{};
    cbDesc.BufferLocation = m_constantBuffers[i]->GetGPUVirtualAddress();
    cbDesc.SizeInBytes = bufferSize;
    CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(m_heapSrvCbv->GetCPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase+i, m_srvcbvDescriptorSize);
    m_device->CreateConstantBufferView(&cbDesc, handleCBV);

    m_cbViews[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSrvCbv->GetGPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase + i, m_srvcbvDescriptorSize);
  }

  // テクスチャの生成
  m_texture = CreateTexture("texture.tga");

  // サンプラーの生成
  D3D12_SAMPLER_DESC samplerDesc{};
  samplerDesc.Filter = D3D12_ENCODE_BASIC_FILTER(
    D3D12_FILTER_TYPE_LINEAR, // min
    D3D12_FILTER_TYPE_LINEAR, // mag
    D3D12_FILTER_TYPE_LINEAR, // mip
    D3D12_FILTER_REDUCTION_TYPE_STANDARD);
  samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samplerDesc.MaxLOD = FLT_MAX;
  samplerDesc.MinLOD = -FLT_MAX;
  samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

  // サンプラー用ディスクリプタヒープの0番目を使用する
  auto descriptorSampler = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heapSampler->GetCPUDescriptorHandleForHeapStart(), SamplerDescriptorBase, m_samplerDescriptorSize);
  m_device->CreateSampler(&samplerDesc, descriptorSampler);
  m_sampler = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSampler->GetGPUDescriptorHandleForHeapStart(), SamplerDescriptorBase, m_samplerDescriptorSize);

  // テクスチャからシェーダーリソースビューの準備.
  auto srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heapSrvCbv->GetCPUDescriptorHandleForHeapStart(), TextureSrvDescriptorBase, m_srvcbvDescriptorSize);
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Texture2D.MipLevels = 1;
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHandle);
  m_srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSrvCbv->GetGPUDescriptorHandleForHeapStart(), TextureSrvDescriptorBase, m_srvcbvDescriptorSize);
}

void CubeApp::Cleanup()
{
  auto index = m_swapchain->GetCurrentBackBufferIndex();
  auto fence = m_frameFences[index];
  auto value = ++m_frameFenceValues[index];
  m_commandQueue->Signal(fence.Get(), value);
  fence->SetEventOnCompletion(value, m_fenceWaitEvent);
  WaitForSingleObject(m_fenceWaitEvent, GpuWaitTimeout);
}

void CubeApp::MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command)
{
  using namespace DirectX;

  // 各行列のセット.
  ShaderParameters shaderParams;
  XMStoreFloat4x4(&shaderParams.mtxWorld, XMMatrixRotationAxis(XMVectorSet(0.0f,1.0f,0.0f,0.0f), XMConvertToRadians(45.0f)));
  auto mtxView = XMMatrixLookAtLH(
    XMVectorSet(0.0f, 3.0f,-5.0f, 0.0f),
    XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
    XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
  );
  auto mtxProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), m_viewport.Width / m_viewport.Height, 0.1f, 100.0f);
  XMStoreFloat4x4(&shaderParams.mtxView, XMMatrixTranspose(mtxView));
  XMStoreFloat4x4(&shaderParams.mtxProj, XMMatrixTranspose(mtxProj));

  // 定数バッファの更新.
  auto& constantBuffer = m_constantBuffers[m_frameIndex];
  {
    void* p;
    CD3DX12_RANGE range(0, 0);
    constantBuffer->Map(0, &range, &p);
    memcpy(p, &shaderParams, sizeof(shaderParams));
    constantBuffer->Unmap(0, nullptr);
  }

  // パイプラインステートのセット
  command->SetPipelineState(m_pipeline.Get());
  // ルートシグネチャのセット
  command->SetGraphicsRootSignature(m_rootSignature.Get());
  // ビューポートとシザーのセット
  command->RSSetViewports(1, &m_viewport);
  command->RSSetScissorRects(1, &m_scissorRect);

  // ディスクリプタヒープをセット.
  ID3D12DescriptorHeap* heaps[] = {
    m_heapSrvCbv.Get(), m_heapSampler.Get()
  };
  command->SetDescriptorHeaps(_countof(heaps), heaps);

  // プリミティブタイプ、頂点・インデックスバッファのセット
  command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  command->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  command->IASetIndexBuffer(&m_indexBufferView);

  command->SetGraphicsRootDescriptorTable(0, m_cbViews[m_frameIndex]);
  command->SetGraphicsRootDescriptorTable(1, m_srv);
  command->SetGraphicsRootDescriptorTable(2, m_sampler);

  // 描画命令の発行
  command->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}

CubeApp::ComPtr<ID3D12Resource1> CubeApp::CreateBuffer(UINT bufferSize, const void* initialData)
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

// 手動で生成版
CubeApp::ComPtr<ID3D12Resource1> CubeApp::CreateTexture(const std::string& fileName)
{
  ComPtr<ID3D12Resource1> texture;
  int texWidth = 0, texHeight = 0, channels = 0;
  auto* pImage = stbi_load(fileName.c_str(), &texWidth, &texHeight, &channels, 0);

  // サイズ・フォーマットからテクスチャリソースのDesc準備
  auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    texWidth, texHeight,
    1,  // 配列サイズ
    1   // ミップマップ数
  );

  // テクスチャ生成
  m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &texDesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(&texture)
  );

  // ステージングバッファ準備
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts;
  UINT numRows;
  UINT64 rowSizeBytes, totalBytes;
  m_device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layouts, &numRows, &rowSizeBytes, &totalBytes);
  ComPtr<ID3D12Resource1> stagingBuffer = CreateBuffer(totalBytes, nullptr);

  // ステージングバッファに画像をコピー
  {
    const UINT imagePitch = texWidth * sizeof(uint32_t);
    void* pBuf;
    CD3DX12_RANGE range(0, 0);
    stagingBuffer->Map(0, &range, &pBuf);
    for (UINT h = 0; h < numRows; ++h)
    {
      auto dst = static_cast<char*>(pBuf) + h * rowSizeBytes;
      auto src = pImage + h * imagePitch;
      memcpy(dst, src, imagePitch);
    }
  }

  // コマンド準備.
  ComPtr<ID3D12GraphicsCommandList> command;
  m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&command));
  ComPtr<ID3D12Fence1> fence;
  m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

  // 転送コマンド
  D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
  dst.pResource = texture.Get();
  dst.SubresourceIndex = 0;
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

  src.pResource = stagingBuffer.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint = layouts;
  command->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

  // コピー後にはテクスチャとしてのステートへ.
  auto barrierTex = CD3DX12_RESOURCE_BARRIER::Transition(
    texture.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
  command->ResourceBarrier(1, &barrierTex);

  command->Close();

  // コマンドの実行
  ID3D12CommandList* cmds[] = { command.Get() };
  m_commandQueue->ExecuteCommandLists(1, cmds);
  // 完了したらシグナルを立てる.
  const UINT64 expected = 1;
  m_commandQueue->Signal(fence.Get(), expected);

  // テクスチャの処理が完了するまで待つ.
  while (expected != fence->GetCompletedValue())
  {
    Sleep(1);
  }

  stbi_image_free(pImage);
  return texture;
}

void CubeApp::PrepareDescriptorHeapForCubeApp()
{
  // CBV/SRV のディスクリプタヒープ
  //  0:シェーダーリソースビュー
  //  1,2 : 定数バッファビュー (FrameBufferCount数分使用)
  UINT count = FrameBufferCount + 1;
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    count,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    0
  };
  m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_heapSrvCbv));
  m_srvcbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // ダイナミックサンプラーのディスクリプタヒープ
  D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc{
    D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
    1,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    0
  };
  m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_heapSampler));
  m_samplerDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}