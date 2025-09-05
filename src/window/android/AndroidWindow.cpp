#include "window/android/AndroidWindow.h"
#include <cstdio>

AndroidWindow::AndroidWindow(const WindowDesc& desc)
  : m_logicalW(desc.logicalWidth), m_logicalH(desc.logicalHeight),
    m_winW(desc.logicalWidth), m_winH(desc.logicalHeight), m_dpr(1.0f) {
  std::puts("[AndroidWindow] stub created (implement NDK glue later)");
  // Later: hook android_native_app_glue loop, read AConfiguration, emit DPR/resize, touch events, lifecycle (pause/resume)
}

AndroidWindow::~AndroidWindow() {}

bool AndroidWindow::ShouldClose() const { return m_shouldClose; }
void AndroidWindow::PollEvents() { /* pump android looper later */ }
void AndroidWindow::SwapBuffers() { /* via GL/Vulkan surface later */ }

Vector2 AndroidWindow::GetLogicalSize() const {
  return Vector2{m_logicalW, m_logicalH};
}
Vector2 AndroidWindow::GetWindowSize() const { return Vector2{m_winW,m_winH}; }
Vector2 AndroidWindow::GetFramebufferSize() const { return Vector2{int(m_winW*m_dpr),int(m_winH*m_dpr)}; }
float AndroidWindow::GetDevicePixelRatio() const { return m_dpr; }

void AndroidWindow::SetCloseCallback(CloseCallback cb){ m_onClose = std::move(cb); }
void AndroidWindow::SetResizeCallback(ResizeCallback cb){ m_onResize = std::move(cb); }

void* AndroidWindow::GetNativeHandle() const { return nullptr; }
