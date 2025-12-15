#define DEBUG_DRAW_IMPLEMENTATION
#include <debug_draw.hpp>
#include <windows.h>


// OpenGLES headers (Simulated/Stubbed for compilation if SDK missing)
#include <EGL/egl.h>
#include <GLES2/gl2.h>


#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "libGLESv2.lib")
#pragma comment(lib, "libEGL.lib")

const char *VS_SRC = R"(
attribute vec3 a_pos;
attribute vec3 a_color;
varying vec3 v_color;
void main() {
    gl_Position = vec4(a_pos, 1.0);
    gl_PointSize = 10.0; // Fixed size for points
    v_color = a_color;
}
)";

const char *PS_SRC = R"(
precision mediump float;
varying vec3 v_color;
void main() {
    gl_FragColor = vec4(v_color, 1.0);
}
)";

class GLES2RenderInterface : public dd::RenderInterface {
public:
  GLuint program = 0;
  GLint a_pos = -1;
  GLint a_color = -1;

  GLES2RenderInterface() { initShaders(); }

  ~GLES2RenderInterface() { glDeleteProgram(program); }

  void initShaders() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &VS_SRC, nullptr);
    glCompileShader(vs);

    GLuint ps = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(ps, 1, &PS_SRC, nullptr);
    glCompileShader(ps);

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, ps);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(ps);

    a_pos = glGetAttribLocation(program, "a_pos");
    a_color = glGetAttribLocation(program, "a_color");
  }

  void drawPointList(const dd::DrawVertex *points, int count,
                     bool depthEnabled) override {
    if (depthEnabled)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);

    glUseProgram(program);

    glEnableVertexAttribArray(a_pos);
    glEnableVertexAttribArray(a_color);

    // Stride is sizeof(dd::DrawVertex)
    glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE, sizeof(dd::DrawVertex),
                          &points[0].point.x);
    glVertexAttribPointer(a_color, 3, GL_FLOAT, GL_FALSE,
                          sizeof(dd::DrawVertex), &points[0].point.r);

    glDrawArrays(GL_POINTS, 0, count);

    glDisableVertexAttribArray(a_pos);
    glDisableVertexAttribArray(a_color);
  }

  void drawLineList(const dd::DrawVertex *lines, int count,
                    bool depthEnabled) override {
    if (depthEnabled)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);

    glUseProgram(program);

    glEnableVertexAttribArray(a_pos);
    glEnableVertexAttribArray(a_color);

    glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE, sizeof(dd::DrawVertex),
                          &lines[0].line.x);
    glVertexAttribPointer(a_color, 3, GL_FLOAT, GL_FALSE,
                          sizeof(dd::DrawVertex), &lines[0].line.r);

    glDrawArrays(GL_LINES, 0, count);

    glDisableVertexAttribArray(a_pos);
    glDisableVertexAttribArray(a_color);
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
                   "DD_GLES",
                   NULL};
  RegisterClassEx(&wc);
  HWND hwnd =
      CreateWindow(wc.lpszClassName, "Debug Draw GLES", WS_OVERLAPPEDWINDOW,
                   100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  // EGL Setup
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(display, nullptr, nullptr);

  EGLint attribs[] = {EGL_RED_SIZE,
                      8,
                      EGL_GREEN_SIZE,
                      8,
                      EGL_BLUE_SIZE,
                      8,
                      EGL_DEPTH_SIZE,
                      24,
                      EGL_RENDERABLE_TYPE,
                      EGL_OPENGL_ES2_BIT,
                      EGL_NONE};
  EGLConfig config;
  EGLint numConfigs;
  eglChooseConfig(display, attribs, &config, 1, &numConfigs);

  EGLSurface surface = eglCreateWindowSurface(display, config, hwnd, nullptr);

  EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext context =
      eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttribs);

  eglMakeCurrent(display, surface, surface, context);

  GLES2RenderInterface renderer;
  dd::initialize(&renderer);

  float t = 0.0f;
  const float dt = 0.016f; // 60 FPS

  MSG msg = {};
  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      // Render Loop
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      dd::clear();

      // Animate a sphere moving in a circle
      float x = std::cos(t) * 5.0f;
      float z = std::sin(t) * 5.0f;

      dd::sphere(ddVec3{x, 0.0f, z}, dd::colors::Yellow, 1.0f);
      dd::line(ddVec3{0, 0, 0}, ddVec3{x, 5.0f, z}, dd::colors::Cyan);

      dd::flush(std::int64_t(t * 1000));

      eglSwapBuffers(display, surface);

      t += dt;
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
  }

  dd::shutdown();

  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(display, context);
  eglDestroySurface(display, surface);
  eglTerminate(display);

  return 0;
}
