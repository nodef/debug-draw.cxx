#define DEBUG_DRAW_IMPLEMENTATION
#include <d3d11.h>
#include <d3dcompiler.h>
#include <debug_draw.hpp>
#include <iostream>
#include <vector>
#include <windows.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Simple HLSL shaders for debug drawing
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
    output.pos = float4(input.pos, 1.0f); // In real app, multiply by ViewProj
    output.color = float4(input.color, 1.0f);
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.color;
}
)";

class D3D11RenderInterface : public dd::RenderInterface {
public:
  ID3D11Device *device = nullptr;
  ID3D11DeviceContext *context = nullptr;
  ID3D11VertexShader *vs = nullptr;
  ID3D11PixelShader *ps = nullptr;
  ID3D11InputLayout *layout = nullptr;
  ID3D11Buffer *vertexBuffer = nullptr;
  size_t vertexBufferSize = 0;

  D3D11RenderInterface(ID3D11Device *dev, ID3D11DeviceContext *ctx)
      : device(dev), context(ctx) {
    initResources();
  }

  ~D3D11RenderInterface() {
    if (vertexBuffer)
      vertexBuffer->Release();
    if (layout)
      layout->Release();
    if (ps)
      ps->Release();
    if (vs)
      vs->Release();
  }

  void initResources() {
    ID3DBlob *vsBlob = nullptr;
    ID3DBlob *psBlob = nullptr;
    ID3DBlob *error = nullptr;

    D3DCompile(HLSL_SRC, strlen(HLSL_SRC), nullptr, nullptr, nullptr, "VS",
               "vs_4_0", 0, 0, &vsBlob, &error);
    if (error) {
      std::cerr << "VS Error: " << (char *)error->GetBufferPointer() << "\n";
      error->Release();
    }
    device->CreateVertexShader(vsBlob->GetBufferPointer(),
                               vsBlob->GetBufferSize(), nullptr, &vs);

    D3DCompile(HLSL_SRC, strlen(HLSL_SRC), nullptr, nullptr, nullptr, "PS",
               "ps_4_0", 0, 0, &psBlob, &error);
    if (error) {
      std::cerr << "PS Error: " << (char *)error->GetBufferPointer() << "\n";
      error->Release();
    }
    device->CreatePixelShader(psBlob->GetBufferPointer(),
                              psBlob->GetBufferSize(), nullptr, &ps);

    D3D11_INPUT_ELEMENT_DESC desc[] = {{"POSITION", 0,
                                        DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                                        D3D11_INPUT_PER_VERTEX_DATA, 0},
                                       {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT,
                                        0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
                                       {"PSIZE", 0, DXGI_FORMAT_R32_FLOAT, 0,
                                        24, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    device->CreateInputLayout(desc, 3, vsBlob->GetBufferPointer(),
                              vsBlob->GetBufferSize(), &layout);

    vsBlob->Release();
    psBlob->Release();

    // Initial vertex buffer (dynamic)
    resizeBuffer(1024);
  }

  void resizeBuffer(size_t size) {
    if (vertexBuffer)
      vertexBuffer->Release();
    vertexBufferSize = size;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = static_cast<UINT>(size * sizeof(dd::DrawVertex));
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&bd, nullptr, &vertexBuffer);
  }

  void drawCommon(const dd::DrawVertex *verts, int count,
                  D3D_PRIMITIVE_TOPOLOGY topology) {
    if (count > vertexBufferSize)
      resizeBuffer(count * 2);

    D3D11_MAPPED_SUBRESOURCE mapped;
    context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, verts, count * sizeof(dd::DrawVertex));
    context->Unmap(vertexBuffer, 0);

    UINT stride = sizeof(dd::DrawVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetInputLayout(layout);
    context->IASetPrimitiveTopology(topology);
    context->VSSetShader(vs, nullptr, 0);
    context->PSSetShader(ps, nullptr, 0);
    context->Draw(count, 0);
  }

  void drawPointList(const dd::DrawVertex *points, int count,
                     bool depthEnabled) override {
    drawCommon(points, count, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  }

  void drawLineList(const dd::DrawVertex *lines, int count,
                    bool depthEnabled) override {
    drawCommon(lines, count, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  }
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_DESTROY)
    PostQuitMessage(0);
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main() {
  // Window Creation
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
                   "DD_D3D11",
                   NULL};
  RegisterClassEx(&wc);
  HWND hwnd =
      CreateWindow(wc.lpszClassName, "Debug Draw D3D11", WS_OVERLAPPEDWINDOW,
                   100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  // D3D11 Init
  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 1;
  sd.BufferDesc.Width = 800;
  sd.BufferDesc.Height = 600;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;

  ID3D11Device *device = nullptr;
  ID3D11DeviceContext *context = nullptr;
  IDXGISwapChain *swapChain = nullptr;
  ID3D11RenderTargetView *renderTargetView = nullptr;
  D3D_FEATURE_LEVEL featureLevel;

  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
      D3D11_SDK_VERSION, &sd, &swapChain, &device, &featureLevel, &context);

  if (FAILED(hr))
    return 1;

  // RTV
  ID3D11Texture2D *backBuffer = nullptr;
  swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backBuffer);
  device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
  backBuffer->Release();
  context->OMSetRenderTargets(1, &renderTargetView, nullptr);

  // Viewport
  D3D11_VIEWPORT vp = {0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f};
  context->RSSetViewports(1, &vp);

  D3D11RenderInterface renderer(device, context);
  dd::initialize(&renderer);

  MSG msg = {};
  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      // Clear
      float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f};
      context->ClearRenderTargetView(renderTargetView, clearColor);

      // Draw
      dd::point(ddVec3{0.0f, 0.0f, 0.0f}, dd::colors::Red, 10.0f);
      dd::line(ddVec3{0.0f, 0.0f, 0.0f}, ddVec3{0.5f, 0.5f, 0.0f},
               dd::colors::Blue);

      dd::flush(0);
      swapChain->Present(1, 0);
    }
  }

  dd::shutdown();

  if (renderTargetView)
    renderTargetView->Release();
  if (swapChain)
    swapChain->Release();
  if (context)
    context->Release();
  if (device)
    device->Release();
  return 0;
}
