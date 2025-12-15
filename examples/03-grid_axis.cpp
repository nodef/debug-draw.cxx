#define DEBUG_DRAW_IMPLEMENTATION
#include <GL/gl.h>
#include <debug_draw.hpp>
#include <iostream>
#include <vector>
#include <windows.h>

#pragma comment(lib, "opengl32.lib")

// OpenGL RenderInterface using Vertex Arrays (compatible with 1.1+)
class OpenGLRenderInterface : public dd::RenderInterface {
public:
  void drawPointList(const dd::DrawVertex *points, int count,
                     bool depthEnabled) override {
    if (depthEnabled)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    // dd::DrawVertex struct layout: x, y, z, r, g, b, ...
    // Stride is sizeof(dd::DrawVertex)
    glVertexPointer(3, GL_FLOAT, sizeof(dd::DrawVertex), &points[0].point.x);
    glColorPointer(3, GL_FLOAT, sizeof(dd::DrawVertex), &points[0].point.r);

    glPointSize(points[0].point.size);
    glDrawArrays(GL_POINTS, 0, count);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
  }

  void drawLineList(const dd::DrawVertex *lines, int count,
                    bool depthEnabled) override {
    if (depthEnabled)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(dd::DrawVertex), &lines[0].line.x);
    glColorPointer(3, GL_FLOAT, sizeof(dd::DrawVertex), &lines[0].line.r);

    glDrawArrays(GL_LINES, 0, count);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
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
                   "DD_GL",
                   NULL};
  RegisterClassEx(&wc);
  HWND hwnd =
      CreateWindow(wc.lpszClassName, "Debug Draw OpenGL", WS_OVERLAPPEDWINDOW,
                   100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);

  // WGL Context
  HDC hdc = GetDC(hwnd);
  PIXELFORMATDESCRIPTOR pfd = {sizeof(PIXELFORMATDESCRIPTOR), 1};
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.iLayerType = PFD_MAIN_PLANE;
  int pf = ChoosePixelFormat(hdc, &pfd);
  SetPixelFormat(hdc, pf, &pfd);
  HGLRC hglrc = wglCreateContext(hdc);
  wglMakeCurrent(hdc, hglrc);

  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  OpenGLRenderInterface renderer;
  dd::initialize(&renderer);

  // Camera setup (ModelView/Projection)
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  // Simple perspective:
  GLdouble fov = 60.0;
  GLdouble aspect = 800.0 / 600.0;
  GLdouble zNear = 0.1;
  GLdouble zFar = 100.0;
  GLdouble fH = tan(fov / 360 * 3.14159) * zNear;
  GLdouble fW = fH * aspect;
  glFrustum(-fW, fW, -fH, fH, zNear, zFar);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0, 0, -10.0f); // Move camera back

  MSG msg = {};
  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      dd::xzSquareGrid(-20.0f, 20.0f, -1.0f, 1.0f, dd::colors::LightGray);

      ddMat4x4 transform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
      dd::axisTriad(transform, 0.5f, 2.0f);

      transform[12] = 5.0f;
      transform[13] = 2.0f;
      dd::axisTriad(transform, 0.2f, 1.0f);

      dd::flush(0);
      SwapBuffers(hdc);
    }
  }

  dd::shutdown();

  wglMakeCurrent(NULL, NULL);
  wglDeleteContext(hglrc);
  ReleaseDC(hwnd, hdc);
  DestroyWindow(hwnd);

  return 0;
}
