#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>
#include <dxgi1_6.h>

#include "d3dx12.h"
#include <wrl.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class D3D12AppBase
{
public:
  template<class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  D3D12AppBase();
  virtual ~D3D12AppBase();

  void Initialize(HWND hWnd);
  void Terminate();

  virtual void Render();
  virtual void Prepare() { }
  virtual void Cleanup() { }
  virtual void MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command) { }

  const UINT GpuWaitTimeout = (10 * 1000);  // 10s
  const UINT FrameBufferCount = 2;

protected:

  virtual void PrepareDescriptorHeaps();
  void PrepareRenderTargetView();
  void CreateDepthBuffer(int width, int height);
  void CreateCommandAllocators();
  void CreateFrameFences();
  void WaitPreviousFrame();
  HRESULT CompileShaderFromFile(
    const std::wstring& fileName, const std::wstring& profile, ComPtr<ID3DBlob>& shaderBlob, ComPtr<ID3DBlob>& errorBlob);

  ComPtr<ID3D12Device> m_device;
  ComPtr<ID3D12CommandQueue> m_commandQueue;
  ComPtr<IDXGISwapChain4> m_swapchain;

  ComPtr<ID3D12DescriptorHeap> m_heapRtv;
  ComPtr<ID3D12DescriptorHeap> m_heapDsv;

  std::vector<ComPtr<ID3D12Resource1>> m_renderTargets;
  ComPtr<ID3D12Resource1> m_depthBuffer;

  CD3DX12_VIEWPORT  m_viewport;
  CD3DX12_RECT m_scissorRect;

  UINT m_rtvDescriptorSize;
  UINT m_srvcbvDescriptorSize;
  std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;

  HANDLE m_fenceWaitEvent;
  std::vector<ComPtr<ID3D12Fence1>> m_frameFences;
  std::vector<UINT64> m_frameFenceValues;

  ComPtr<ID3D12GraphicsCommandList> m_commandList;

  UINT m_frameIndex;
};

