#define DEBUG_DRAW_IMPLEMENTATION
#include <d3d12.h>
#include <d3dcompiler.h>
#include <debug_draw.hpp>
#include <dxgi1_4.h>
#include <iostream>
#include <vector>
#include <windows.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr; // Not using WRL to keep it dependency-free (using
                              // raw pointers)

const char *HLSL_SRC = R"(
struct VS_INPUT {
    float3 pos : POSITION;
    float3 color : COLOR;
    float size : PSIZE;
};
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.color = float4(input.color, 1.0f);
    return output;
}
float4 PS(PS_INPUT input) : SV_Target {
    return input.color;
}
)";

class D3D12RenderInterface : public dd::RenderInterface {
public:
  ID3D12Device *device;
  ID3D12GraphicsCommandList *cmdList;
  ID3D12RootSignature *rootSig = nullptr;
  ID3D12PipelineState *pso = nullptr;
  ID3D12Resource *uploadBuffer = nullptr;
  UINT8 *mappedPtr = nullptr;
  size_t currentOffset = 0;
  size_t bufferSize = 1024 * 1024; // 1MB

  D3D12RenderInterface(ID3D12Device *dev, ID3D12GraphicsCommandList *list)
      : device(dev), cmdList(list) {
    initResources();
  }

  ~D3D12RenderInterface() {
    if (rootSig)
      rootSig->Release();
    if (pso)
      pso->Release();
    if (uploadBuffer) {
      uploadBuffer->Unmap(0, nullptr);
      uploadBuffer->Release();
    }
  }

  void initResources() {
    // Root Signature
    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ID3DBlob *sigBlob;
    ID3DBlob *error;
    D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                &sigBlob, &error);
    device->CreateRootSignature(0, sigBlob->GetBufferPointer(),
                                sigBlob->GetBufferSize(),
                                IID_PPV_ARGS(&rootSig));
    sigBlob->Release();

    // Shaders
    ID3DBlob *vsBlob;
    D3DCompile(HLSL_SRC, strlen(HLSL_SRC), nullptr, nullptr, nullptr, "VS",
               "vs_5_0", 0, 0, &vsBlob, nullptr);
    ID3DBlob *psBlob;
    D3DCompile(HLSL_SRC, strlen(HLSL_SRC), nullptr, nullptr, nullptr, "PS",
               "ps_5_0", 0, 0, &psBlob, nullptr);

    // PSO
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"PSIZE", 0, DXGI_FORMAT_R32_FLOAT, 0, 24,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {inputElementDescs, 3};
    psoDesc.pRootSignature = rootSig;
    psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
    psoDesc.RasterizerState = {D3D12_FILL_MODE_SOLID,
                               D3D12_CULL_MODE_NONE,
                               FALSE,
                               D3D12_DEFAULT_DEPTH_BIAS,
                               D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
                               D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
                               TRUE,
                               FALSE,
                               FALSE,
                               0,
                               D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF};
    psoDesc.BlendState = {FALSE, FALSE};
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
        FALSE,
        FALSE,
        D3D12_BLEND_ONE,
        D3D12_BLEND_ZERO,
        D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE,
        D3D12_BLEND_ZERO,
        D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL};
    psoDesc.BlendState.RenderTarget[0] = defaultRenderTargetBlendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; // Needs switching for lines in
                                             // real logic
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));

    vsBlob->Release();
    psBlob->Release();

    // Upload Heap
    D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_UPLOAD,
                                       D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                       D3D12_MEMORY_POOL_UNKNOWN, 1, 1};
    D3D12_RESOURCE_DESC resDesc = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                   0,
                                   (UINT64)bufferSize,
                                   1,
                                   1,
                                   1,
                                   DXGI_FORMAT_UNKNOWN,
                                   {1, 0},
                                   D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                   D3D12_RESOURCE_FLAG_NONE};
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                    IID_PPV_ARGS(&uploadBuffer));
    uploadBuffer->Map(0, nullptr, (void **)&mappedPtr);
  }

  void drawCommon(const dd::DrawVertex *verts, int count,
                  D3D_PRIMITIVE_TOPOLOGY topology) {
    if (currentOffset + count * sizeof(dd::DrawVertex) > bufferSize) {
      currentOffset = 0; // Wrap around for simplicity in this example
    }

    memcpy(mappedPtr + currentOffset, verts, count * sizeof(dd::DrawVertex));

    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = uploadBuffer->GetGPUVirtualAddress() + currentOffset;
    vbv.StrideInBytes = sizeof(dd::DrawVertex);
    vbv.SizeInBytes = count * sizeof(dd::DrawVertex);

    cmdList->SetGraphicsRootSignature(rootSig);
    cmdList->SetPipelineState(pso);
    cmdList->IASetPrimitiveTopology(topology);
    cmdList->IASetVertexBuffers(0, 1, &vbv);
    cmdList->DrawInstanced(count, 1, 0, 0);

    currentOffset += count * sizeof(dd::DrawVertex);
  }

  void drawPointList(const dd::DrawVertex *points, int count,
                     bool depthEnabled) override {
    drawCommon(points, count, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
  }

  void drawLineList(const dd::DrawVertex *lines, int count,
                    bool depthEnabled) override {
    drawCommon(lines, count, D3D_PRIMITIVE_TOPOLOGY_LINELIST);
  }
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_DESTROY)
    PostQuitMessage(0);
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main() {
  // Window Setup
  WNDCLASSEX wc = {sizeof(WNDCLASSEX),
                   CS_CLASSDC,
                   WndProc,
                   0L,
                   0L,
                   GetModuleHandle(NULL),
                   NULL,
                   NULL,
                   NULL,
                   NULL,
                   "DD_D3D12",
                   NULL};
  RegisterClassEx(&wc);
  HWND hwnd =
      CreateWindow(wc.lpszClassName, "Debug Draw D3D12", WS_OVERLAPPEDWINDOW,
                   100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  // D3D12 Setup
  ID3D12Device *device = nullptr;
  IDXGIFactory4 *factory = nullptr;
  IDXGISwapChain3 *swapChain = nullptr;
  ID3D12CommandQueue *commandQueue = nullptr;
  ID3D12GraphicsCommandList *cmdList = nullptr;
  ID3D12CommandAllocator *allocator = nullptr;
  ID3D12DescriptorHeap *rtvHeap = nullptr;
  ID3D12Resource *renderTargets[2];
  UINT rtvDescriptorSize = 0;
  ID3D12Fence *fence = nullptr;
  UINT64 fenceValue = 0;
  HANDLE fenceEvent = nullptr;

  CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));

  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.BufferCount = 2;
  swapChainDesc.Width = 800;
  swapChainDesc.Height = 600;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.SampleDesc.Count = 1;

  IDXGISwapChain1 *tempSwapChain;
  factory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr,
                                  nullptr, &tempSwapChain);
  tempSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain));
  tempSwapChain->Release();

  device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 IID_PPV_ARGS(&allocator));
  device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator,
                            nullptr, IID_PPV_ARGS(&cmdList));
  cmdList->Close(); // Start closed, reset in loop

  // RTV Heap
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = 2;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
  rtvDescriptorSize =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // Create RTVs
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
      rtvHeap->GetCPUDescriptorHandleForHeapStart();
  for (UINT i = 0; i < 2; i++) {
    swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
    device->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandle);
    rtvHandle.ptr += rtvDescriptorSize;
  }

  // Fence
  device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  D3D12RenderInterface renderer(device, cmdList);
  dd::initialize(&renderer);

  int frameIndex = 0;
  MSG msg = {};
  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      frameIndex = swapChain->GetCurrentBackBufferIndex();
      allocator->Reset();
      cmdList->Reset(allocator, nullptr);

      // Barrier: Present -> RenderTarget
      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = renderTargets[frameIndex];
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      cmdList->ResourceBarrier(1, &barrier);

      D3D12_CPU_DESCRIPTOR_HANDLE rtvVal =
          rtvHeap->GetCPUDescriptorHandleForHeapStart();
      rtvVal.ptr += frameIndex * rtvDescriptorSize;
      cmdList->OMSetRenderTargets(1, &rtvVal, FALSE, nullptr);

      float clearColor[] = {0.2f, 0.2f, 0.2f, 1.0f};
      cmdList->ClearRenderTargetView(rtvVal, clearColor, 0, nullptr);

      D3D12_VIEWPORT vp = {0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f};
      D3D12_RECT scissor = {0, 0, 800, 600};
      cmdList->RSSetViewports(1, &vp);
      cmdList->RSSetScissorRects(1, &scissor);

      // Draw Debug Draw Items
      dd::box(ddVec3{-2, 0, 0}, dd::colors::CornflowerBlue, 1.0f, 1.0f, 1.0f);
      dd::sphere(ddVec3{0, 0, 0}, dd::colors::Lime, 1.0f);
      dd::cone(ddVec3{2, 0, 0}, ddVec3{0, 1, 0}, dd::colors::Orange, 1.0f,
               0.1f);
      dd::flush(0); // This records commands

      // Barrier: RenderTarget -> Present
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
      cmdList->ResourceBarrier(1, &barrier);

      cmdList->Close();
      ID3D12CommandList *ppCommandLists[] = {cmdList};
      commandQueue->ExecuteCommandLists(1, ppCommandLists);
      swapChain->Present(1, 0);

      // Wait for GPU (Poor man's sync)
      const UINT64 fenceRef = fenceValue;
      commandQueue->Signal(fence, fenceRef);
      fenceValue++;
      if (fence->GetCompletedValue() < fenceRef) {
        fence->SetEventOnCompletion(fenceRef, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
      }
    }
  }

  dd::shutdown();
  // Cleanup skipped for brevity (release logic)
  return 0;
}
