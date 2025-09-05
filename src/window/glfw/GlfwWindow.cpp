#include "window/glfw/GlfwWindow.h"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <cstdio>

static Key mapKey(int glfwKey) {
  // Minimal map (1:1 where possible)
  if (glfwKey >= GLFW_KEY_0 && glfwKey <= GLFW_KEY_9) return Key(uint16_t('0' + (glfwKey - GLFW_KEY_0)));
  if (glfwKey >= GLFW_KEY_A && glfwKey <= GLFW_KEY_Z) return Key(uint16_t('A' + (glfwKey - GLFW_KEY_A)));
  switch (glfwKey) {
  case GLFW_KEY_ESCAPE: return Key::Escape;
  case GLFW_KEY_ENTER:  return Key::Enter;
  case GLFW_KEY_TAB:    return Key::Tab;
  case GLFW_KEY_BACKSPACE: return Key::Backspace;
  case GLFW_KEY_LEFT: return Key::Left; case GLFW_KEY_RIGHT: return Key::Right;
  case GLFW_KEY_UP:   return Key::Up;   case GLFW_KEY_DOWN:  return Key::Down;
  case GLFW_KEY_F1: return Key::F1; case GLFW_KEY_F2: return Key::F2; // …extend as needed
  default: return Key::Unknown;
  }
}

static MouseButton mapMouse(int b) {
  switch (b) {
  case GLFW_MOUSE_BUTTON_LEFT: return MouseButton::Left;
  case GLFW_MOUSE_BUTTON_RIGHT: return MouseButton::Right;
  case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
  case GLFW_MOUSE_BUTTON_4: return MouseButton::Button4;
  default: return MouseButton::Button5;
  }
}

static int s_glfwRefCount = 0;

GlfwWindow::GlfwWindow(const FactoryDesc& desc){
    if(s_glfwRefCount++ == 0){
        if(!glfwInit()){
            s_glfwRefCount--;
            throw std::runtime_error("glfwInit failed");
        }
    }

    // Use GL context if requested; otherwise NO_API so Vulkan/other can attach later.
    if (desc.graphics_api == GraphicsAPI::OpenGL) {
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, desc.glMajor);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, desc.glMinor);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
      glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    } else {
      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);

    int initialW = desc.windowWidth > 0 ? desc.windowWidth : desc.logicalWidth;
    int initialH = desc.windowHeight > 0 ? desc.windowHeight : desc.logicalHeight;

    if(desc.fullscreen){
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(mon);

        initialW = mode->width;
        initialH = mode->height;
        m_Window = glfwCreateWindow(initialW, initialH, desc.title.c_str(), mon, nullptr);
    } else{

        m_Window = glfwCreateWindow(initialW, initialH, desc.title.c_str(), nullptr, nullptr);
    }

    if(!m_Window){
        if(--s_glfwRefCount == 0) glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    // Store pointer for callbacks
    glfwSetWindowUserPointer(m_Window, this);

    // Core callbacks
    glfwSetFramebufferSizeCallback(m_Window, &GlfwWindow::framebufferSizeCB);
    glfwSetWindowCloseCallback(m_Window, &GlfwWindow::windowCloseCB);
    glfwSetWindowContentScaleCallback(m_Window, &GlfwWindow::contentScaleCB);

    // Input callbacks
    glfwSetKeyCallback(m_Window, &keyCB);
    glfwSetCharCallback(m_Window, &charCB);
    glfwSetMouseButtonCallback(m_Window, &GlfwWindow::mouseButtonCB);
    glfwSetCursorPosCallback(m_Window, &GlfwWindow::cursorPosCB);
    glfwSetScrollCallback(m_Window, &GlfwWindow::scrollCB);

    m_logicalW = desc.logicalWidth;
    m_logicalH = desc.logicalHeight;
    m_Vsync = desc.vsync;

    // Compute initial DPR
    updateDevicePixelRatio();

    m_HasGL = (desc.graphics_api == GraphicsAPI::OpenGL);
}

GlfwWindow::~GlfwWindow(){
    if(m_Window){
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }

    if(--s_glfwRefCount == 0){
        glfwTerminate();
    }
}

bool GlfwWindow::ShouldClose() const {
    return glfwWindowShouldClose(m_Window);
}

void GlfwWindow::PollEvents() {
    glfwPollEvents();
}

void GlfwWindow::SwapBuffers() {
  if(m_HasGL) glfwSwapBuffers(m_Window);
}

std::pair<float, float> GlfwWindow::GetLogicalSize() const{
    return {m_logicalW, m_logicalH};
}

std::pair<float, float> GlfwWindow::GetWindowSize() const {
    int w,h;
    glfwGetWindowSize(m_Window, &w, &h);
    return {w,h};
}

std::pair<float, float> GlfwWindow::GetFramebufferSize() const {
    int w,h;
    glfwGetFramebufferSize(m_Window, &w, &h);
    return {w,h};
}

float GlfwWindow::GetDevicePixelRatio() const { return m_dpr; }

void GlfwWindow::SetCloseCallback(CloseCallback cb) { m_OnClose = std::move(cb); }
void GlfwWindow::SetResizeCallback(ResizeCallback cb) { m_OnResize = std::move(cb); }

void* GlfwWindow::GetNativeHandle() const {
#if defined(_WIN32)
    // return glfwGetWin32Window(m_Window);
    return m_Window;
#elif defined(__APPLE__)
    return glfwGetCocoaWindow(m_Window);
#else
    // return (void*)glfwGetX11Window(m_Window);
    return m_Window;
#endif
}

bool GlfwWindow::EnsureGLContext(int /*major*/, int /*minor*/, bool /*debug*/) {
  if (!m_HasGL) return false;
  glfwMakeContextCurrent(m_Window);
  return true;
}
GlfwWindow::GLProc GlfwWindow::GetGLProcLoader() const {
  return (GLProc)glfwGetProcAddress;
}
void GlfwWindow::SetSwapInterval(int interval) {
  if (m_HasGL) glfwSwapInterval(interval);
}

void GlfwWindow::updateDevicePixelRatio() {
    int fbW=0, fbH=0;
    glfwGetFramebufferSize(m_Window, &fbW, &fbH);

    // Protect against division by zero / first frame
    float dprX = (m_logicalW > 0) ? (float)fbW / (float)m_logicalW : 1.0f;
    float dprY = (m_logicalH > 0) ? (float)fbH / (float)m_logicalH : 1.0f;
    m_dpr = (dprX + dprY) * 0.5f;
}

// ------- Callbacks ---------
void GlfwWindow::framebufferSizeCB(GLFWwindow* win, int w, int h) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    self->updateDevicePixelRatio();
    self->Events().Emit(EWindowResize{w,h, self->m_dpr});
    if (self->m_OnResize) self->m_OnResize(w, h, self->m_dpr);
}

void GlfwWindow::windowCloseCB(GLFWwindow* win) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self)
        return;
    self->Events().EmitClose();
    if (self->m_OnClose) self->m_OnClose();
    glfwSetWindowShouldClose(win, 1);
}

void GlfwWindow::contentScaleCB(GLFWwindow* win, float /*xscale*/, float /*yscale*/) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    self->updateDevicePixelRatio();
    self->Events().Emit(EContentScale{self->m_dpr});
}

void GlfwWindow::keyCB(GLFWwindow* win, int key, int, int action, int) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    Key k = mapKey(key);
    if (k == Key::Unknown) return;
    bool down = (action != GLFW_RELEASE);
    self->Events().Emit(EKey{ k, action==GLFW_REPEAT }, down);
}
void GlfwWindow::charCB(GLFWwindow* win, unsigned int codepoint) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    self->Events().Emit(EKeyChar{ codepoint });
}
void GlfwWindow::mouseButtonCB(GLFWwindow* win, int button, int action, int) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    double xw, yw; glfwGetCursorPos(win, &xw, &yw);
    float x = self->GetLogicalX((float)xw);
    float y = self->GetLogicalY((float)yw);
    self->Events().Emit(EMouseButton{ mapMouse(button), x, y }, action == GLFW_PRESS);
}
void GlfwWindow::cursorPosCB(GLFWwindow* win, double xw, double yw) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    float x = self->GetLogicalX((float)xw);
    float y = self->GetLogicalY((float)yw);
    self->Events().Emit(EMouseMove{ x, y });
}
void GlfwWindow::scrollCB(GLFWwindow* win, double dx, double dy) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    self->Events().Emit(EMouseWheel{ (float)dx, (float)dy });
}
