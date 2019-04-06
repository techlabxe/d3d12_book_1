#include "ModelApp.h"
#include "streamreader.h"
#include <DirectXTex.h>

using namespace DirectX;
using namespace std;

void ModelApp::Prepare()
{
  // モデルデータの読み込み
  auto modelFilePath = experimental::filesystem::path("alicia-solid.vrm");
  if (modelFilePath.is_relative())
  {
    auto current = experimental::filesystem::current_path();
    current /= modelFilePath;
    current.swap(modelFilePath);
  }
  auto reader = make_unique<StreamReader>(modelFilePath.parent_path());
  auto glbStream = reader->GetInputStream(modelFilePath.filename().u8string());
  auto glbResourceReader = make_shared<Microsoft::glTF::GLBResourceReader>(std::move(reader), std::move(glbStream));
  auto document = Microsoft::glTF::Deserialize(glbResourceReader->GetJson());

  // マテリアル数分のSRVディスクリプタが必要になるのでここで準備.
  PrepareDescriptorHeapForModelApp(UINT(document.materials.Elements().size()));

  MakeModelGeometry(document, glbResourceReader);
  MakeModelMaterial(document, glbResourceReader);

  // シェーダーをコンパイル.
  HRESULT hr;
  ComPtr<ID3DBlob> errBlob;
  hr = CompileShaderFromFile(L"shaderVS.hlsl", L"vs_6_0", m_vs, errBlob);
  if (FAILED(hr))
  {
    OutputDebugStringA((const char*)errBlob->GetBufferPointer());
  }
  hr = CompileShaderFromFile(L"shaderOpaquePS.hlsl", L"ps_6_0", m_psOpaque, errBlob);
  if (FAILED(hr))
  {
    OutputDebugStringA((const char*)errBlob->GetBufferPointer());
  }
  hr = CompileShaderFromFile(L"shaderAlphaPS.hlsl", L"ps_6_0", m_psAlpha, errBlob);
  if (FAILED(hr))
  {
    OutputDebugStringA((const char*)errBlob->GetBufferPointer());
  }

  m_srvDescriptorBase = FrameBufferCount;

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

  m_pipelineOpaque = CreateOpaquePSO();
  m_pipelineAlpha = CreateAlphaPSO();


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
}

void ModelApp::Cleanup()
{
  WaitGPU();
}

void ModelApp::MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command)
{
  using namespace DirectX;
  using namespace Microsoft::glTF;

  // 各行列のセット.
  ShaderParameters shaderParams;
  XMStoreFloat4x4(&shaderParams.mtxWorld, XMMatrixRotationAxis(XMVectorSet(0.0f,1.0f,0.0f,0.0f), XMConvertToRadians(0.0f)));
  auto mtxView = XMMatrixLookAtLH(
    XMVectorSet(0.0f, 1.5f, -1.0f, 0.0f),
    XMVectorSet(0.0f, 1.25f, 0.0f, 0.0f),
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

  for (auto mode : { ALPHA_OPAQUE, ALPHA_MASK, ALPHA_BLEND })
  {
    for (const auto& mesh : m_model.meshes)
    {
      auto& material = m_model.materials[mesh.materialIndex];
      // 対応するポリゴンメッシュのみを描画する
      if (material.alphaMode == mode)
      {
        continue;
      }
      // モードに応じて使用するパイプラインステートを変える.
      switch (mode)
      {
      case Microsoft::glTF::ALPHA_OPAQUE:
        command->SetPipelineState(m_pipelineOpaque.Get());
        break;
      case Microsoft::glTF::ALPHA_MASK:
        command->SetPipelineState(m_pipelineOpaque.Get());
        break;
      case Microsoft::glTF::ALPHA_BLEND:
        command->SetPipelineState(m_pipelineAlpha.Get());
        break;
      }

      // プリミティブタイプ、頂点・インデックスバッファのセット
      command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      command->IASetVertexBuffers(0, 1, &mesh.vertexBuffer.vertexView);
      command->IASetIndexBuffer(&mesh.indexBuffer.indexView);

      command->SetGraphicsRootDescriptorTable(0, m_cbViews[m_frameIndex]);
      command->SetGraphicsRootDescriptorTable(1, material.shaderResourceView);
      command->SetGraphicsRootDescriptorTable(2, m_sampler);

      // このメッシュを描画
      command->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
    }
  }

}

void ModelApp::WaitGPU()
{
  HRESULT hr;
  const auto finishExpected = m_frameFenceValues[m_frameIndex];
  hr = m_commandQueue->Signal(m_frameFences[m_frameIndex].Get(), finishExpected);
  if (FAILED(hr))
  {
    throw std::runtime_error("Failed Signal(WaitGPU)");
  }
  m_frameFences[m_frameIndex]->SetEventOnCompletion(finishExpected, m_fenceWaitEvent);
  WaitForSingleObject(m_fenceWaitEvent, GpuWaitTimeout);
  m_frameFenceValues[m_frameIndex] = finishExpected + 1;
}
ModelApp::ComPtr<ID3D12Resource1> ModelApp::CreateBuffer(UINT bufferSize, const void* initialData)
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

ModelApp::TextureObject ModelApp::CreateTextureFromMemory(const std::vector<char>& imageData)
{
  // VRM なので png/jpeg などのファイルを想定し、WIC で読み込む.
  ComPtr<ID3D12Resource1> staging;
  HRESULT hr;
  ScratchImage image;
  hr = LoadFromWICMemory(imageData.data(), imageData.size(), 0, nullptr, image);
  if (FAILED(hr))
  {
    throw std::runtime_error("Failed LoadFromWICMemory");
  }
  auto metadata = image.GetMetadata();
  std::vector<D3D12_SUBRESOURCE_DATA> subresources;

  DirectX::PrepareUpload(
    m_device.Get(), image.GetImages(), image.GetImageCount(),
    metadata, subresources);

  ComPtr<ID3D12Resource> texture;
  DirectX::CreateTexture(m_device.Get(), metadata, &texture);

  // ステージングバッファの準備
  const auto totalBytes = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
  m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(totalBytes),
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&staging)
  );

  // 転送処理
  ComPtr<ID3D12GraphicsCommandList> command;
  m_device->CreateCommandList(
    0, D3D12_COMMAND_LIST_TYPE_DIRECT, 
    m_commandAllocators[m_frameIndex].Get(), 
    nullptr, IID_PPV_ARGS(&command));

  UpdateSubresources(
    command.Get(),
    texture.Get(), staging.Get(),
    0, 0, uint32_t(subresources.size()), subresources.data()
  );

  // リソースバリアのセット
  auto barrierTex = CD3DX12_RESOURCE_BARRIER::Transition(
    texture.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
  command->ResourceBarrier(1, &barrierTex);

  // コマンドの実行.
  command->Close();
  ID3D12CommandList* cmds[] = { command.Get() };
  m_commandQueue->ExecuteCommandLists(1, cmds);
  m_frameFenceValues[m_frameIndex]++;
  WaitGPU();

  TextureObject ret;
  texture.As(&ret.texture);
  ret.format = metadata.format;

  return ret;
}

void ModelApp::PrepareDescriptorHeapForModelApp(UINT materialCount)
{
  m_srvDescriptorBase = FrameBufferCount;
  UINT countCBVSRVDescriptors = materialCount + FrameBufferCount;

  // CBV/SRV のディスクリプタヒープ
  //  0, 1:定数バッファビュー (FrameBufferCount数分使用)
  //  FrameBufferCount ～ :  各マテリアルのテクスチャ(シェーダーリソースビュー)
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    countCBVSRVDescriptors,
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

void ModelApp::MakeModelGeometry(const Microsoft::glTF::Document& doc, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader)
{
  using namespace Microsoft::glTF;
  for (const auto& mesh : doc.meshes.Elements())
  {
    for (const auto& meshPrimitive : mesh.primitives)
    {
      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;

      // 頂点位置情報アクセッサの取得
      auto& idPos = meshPrimitive.GetAttributeAccessorId(ACCESSOR_POSITION);
      auto& accPos = doc.accessors.Get(idPos);
      // 法線情報アクセッサの取得
      auto& idNrm = meshPrimitive.GetAttributeAccessorId(ACCESSOR_NORMAL);
      auto& accNrm = doc.accessors.Get(idNrm);
      // テクスチャ座標情報アクセッサの取得
      auto& idUV = meshPrimitive.GetAttributeAccessorId(ACCESSOR_TEXCOORD_0);
      auto& accUV = doc.accessors.Get(idUV);
      // 頂点インデックス用アクセッサの取得
      auto& idIndex = meshPrimitive.indicesAccessorId;
      auto& accIndex = doc.accessors.Get(idIndex);

      // アクセッサからデータ列を取得
      auto vertPos = reader->ReadBinaryData<float>(doc, accPos);
      auto vertNrm = reader->ReadBinaryData<float>(doc, accNrm);
      auto vertUV = reader->ReadBinaryData<float>(doc, accUV);

      auto vertexCount = accPos.count;
      for (uint32_t i = 0; i < vertexCount; ++i)
      {
        // 頂点データの構築
        int vid0 = 3 * i, vid1 = 3 * i + 1, vid2 = 3 * i + 2;
        int tid0 = 2 * i, tid1 = 2 * i + 1;
        vertices.emplace_back(
          Vertex{ 
            XMFLOAT3(vertPos[vid0], vertPos[vid1],vertPos[vid2]),
            XMFLOAT3(vertNrm[vid0], vertNrm[vid1],vertNrm[vid2]),
            XMFLOAT2(vertUV[tid0],vertUV[tid1])
          }
        );
      }
      // インデックスデータ
      indices = reader->ReadBinaryData<uint32_t>(doc, accIndex);

      auto vbSize = UINT(sizeof(Vertex) * vertices.size());
      auto ibSize = UINT(sizeof(uint32_t) * indices.size());
      ModelMesh modelMesh;
      auto vb = CreateBuffer(vbSize, vertices.data());
      D3D12_VERTEX_BUFFER_VIEW vbView;
      vbView.BufferLocation = vb->GetGPUVirtualAddress();
      vbView.SizeInBytes = vbSize;
      vbView.StrideInBytes = sizeof(Vertex);
      modelMesh.vertexBuffer.buffer = vb;
      modelMesh.vertexBuffer.vertexView = vbView;

      auto ib = CreateBuffer(ibSize, indices.data());
      D3D12_INDEX_BUFFER_VIEW ibView;
      ibView.BufferLocation = ib->GetGPUVirtualAddress();
      ibView.Format = DXGI_FORMAT_R32_UINT;
      ibView.SizeInBytes = ibSize;
      modelMesh.indexBuffer.buffer = ib;
      modelMesh.indexBuffer.indexView = ibView;

      modelMesh.vertexCount = UINT(vertices.size());
      modelMesh.indexCount = UINT(indices.size());
      modelMesh.materialIndex = int(doc.materials.GetIndex(meshPrimitive.materialId));
      m_model.meshes.push_back(modelMesh);
    }
  }
}

void ModelApp::MakeModelMaterial(const Microsoft::glTF::Document& doc, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader)
{
  int textureIndex = 0;
  for (auto& m : doc.materials.Elements())
  {
    auto textureId = m.metallicRoughness.baseColorTexture.textureId;
    if (textureId.empty())
    {
      textureId = m.normalTexture.textureId;
    }
    auto& texture = doc.textures.Get(textureId);
    auto& image = doc.images.Get(texture.imageId);
    auto imageBufferView = doc.bufferViews.Get(image.bufferViewId);
    auto imageData = reader->ReadBinaryData<char>(doc, imageBufferView);

    // imageData が画像データ
    Material material{};
    material.alphaMode = m.alphaMode;
    auto texObj = CreateTextureFromMemory(imageData);
    material.texture = texObj.texture;

    // シェーダーリソースビューの生成.
    auto descriptorIndex = m_srvDescriptorBase + textureIndex;
    auto srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      m_heapSrvCbv->GetCPUDescriptorHandleForHeapStart(), 
      descriptorIndex, 
      m_srvcbvDescriptorSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Format = texObj.format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    m_device->CreateShaderResourceView(
      texObj.texture.Get(), &srvDesc, srvHandle);
    material.shaderResourceView = 
      CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_heapSrvCbv->GetGPUDescriptorHandleForHeapStart(),
        descriptorIndex, 
        m_srvcbvDescriptorSize);

    m_model.materials.push_back(material);

    textureIndex++;
  }
}

ModelApp::ComPtr<ID3D12PipelineState> ModelApp::CreateOpaquePSO()
{
  // インプットレイアウト
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, offsetof(Vertex,Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex,UV), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
  };

  // パイプラインステートオブジェクトの生成.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
  // シェーダーのセット
  psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.Get());
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_psOpaque.Get());
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

  ComPtr<ID3D12PipelineState> pipeline;
  HRESULT hr;
  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
  if (FAILED(hr))
  {
    throw std::runtime_error("CreateGraphicsPipelineState failed");
  }
  return pipeline;
}

ModelApp::ComPtr<ID3D12PipelineState> ModelApp::CreateAlphaPSO()
{
  // インプットレイアウト
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, offsetof(Vertex,Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex,UV), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
  };

  // パイプラインステートオブジェクトの生成.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
  // シェーダーのセット
  psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.Get());
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_psAlpha.Get());
  // ブレンドステート設定(半透明用設定)
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  auto& target = psoDesc.BlendState.RenderTarget[0];
  target.BlendEnable = TRUE;
  target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
  target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  target.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  // ラスタライザーステート
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  // 出力先は1ターゲット
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  // デプスバッファのフォーマットを設定
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  // デプステストのみを有効化(書き込みを行わない)
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };

  // ルートシグネチャのセット
  psoDesc.pRootSignature = m_rootSignature.Get();
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  // マルチサンプル設定
  psoDesc.SampleDesc = { 1,0 };
  psoDesc.SampleMask = UINT_MAX; // これを忘れると絵が出ない＆警告も出ないので注意.

  ComPtr<ID3D12PipelineState> pipeline;
  HRESULT hr;
  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
  if (FAILED(hr))
  {
    throw std::runtime_error("CreateGraphicsPipelineState failed");
  }
  return pipeline;
}
