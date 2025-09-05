#pragma once

#include "events/EventBus.h"
#include "window/IWindow.h"
#include "input/Key.h"
#include "window/WindowDesc.h"
#include <string>

struct GLFWwindow;

class GlfwWindow : public IWindow{
public:
  explicit GlfwWindow(const WindowDesc& desc);
  ~GlfwWindow() override;

  WindowAPI API() const override { return WindowAPI::GLFW;}

  bool ShouldClose() const override;
  void PollEvents() override;
  void SwapBuffers() override;

  glm::vec2 GetLogicalSize() const override;
  glm::vec2 GetWindowSize() const override;
  glm::vec2 GetFramebufferSize() const override;
  float GetDevicePixelRatio() const override;

  void SetCloseCallback(CloseCallback cb) override;
  void SetResizeCallback(ResizeCallback cb) override;

  EventBus &Events() override { return m_Events; }

  // GL helpers
  bool EnsureGLContext(int major, int minor, bool debug) override;
  GLProc GetGLProcLoader() const override;
  void SetSwapInterval(int interval) override;
  bool IsVsyncEnabled() const override { return m_Vsync; }

  void* GetNativeHandle() const override;

private:
  void updateDevicePixelRatio();
  
  static void framebufferSizeCB(GLFWwindow* win, int w, int h);
  static void windowCloseCB(GLFWwindow* win);
  static void contentScaleCB(GLFWwindow *win, float xscale, float yscale);
  static void keyCB(GLFWwindow* win, int key, int scancode, int action, int mods);
  static void charCB(GLFWwindow* win, unsigned int codepoint);
  static void mouseButtonCB(GLFWwindow* win, int button, int action, int mods);
  static void cursorPosCB(GLFWwindow* win, double x, double y);
  static void scrollCB(GLFWwindow* win, double dx, double dy);

private:
  GLFWwindow* m_Window = nullptr;
  int m_logicalW = 360;
  int m_logicalH = 640;
  float m_dpr = 1.0f;
  EventBus m_Events;

  bool  m_HasGL = false;
  bool  m_Vsync = true;

  CloseCallback m_OnClose;
  ResizeCallback m_OnResize;
};
