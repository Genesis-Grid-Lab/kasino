#include "window/glfw/GlfwWindow.h"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <cstdio>

static int s_glfwRefCount = 0;

GlfwWindow::GlfwWindow(const WindowDesc& desc){
    if(s_glfwRefCount++ == 0){
        if(!glfwInit()){
            s_glfwRefCount--;
            throw std::runtime_error("glfwInit failed");
        }
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
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

    // callbacks
    glfwSetFramebufferSizeCallback(m_Window, &GlfwWindow::framebufferSizeCB);
    glfwSetWindowCloseCallback(m_Window, &GlfwWindow::windowCloseCB);
    glfwSetWindowContentScaleCallback(m_Window, &GlfwWindow::contentScaleCB);

    m_logicalW = desc.logicalWidth;
    m_logicalH = desc.logicalHeight;

    // Compute initial DPR
    updateDevicePixelRatio();
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
    // No GL context here; no-op. If you create a GL context later, use glfwSwapBuffers.
}

Vector2 GlfwWindow::GetLogicalSize() const{
    return Vector2{m_logicalW, m_logicalH};
}

Vector2 GlfwWindow::GetWindowSize() const {
    int w,h;
    glfwGetWindowSize(m_Window, &w, &h);
    return Vector2{w,h};
}

Vector2 GlfwWindow::GetFramebufferSize() const {
    int w,h;
    glfwGetFramebufferSize(m_Window, &w, &h);
    return Vector2{w,h};
}

float GlfwWindow::GetDevicePixelRatio() const { return m_dpr; }

void GlfwWindow::SetCloseCallback(CloseCallback cb) { m_onClose = std::move(cb); }
void GlfwWindow::SetResizeCallback(ResizeCallback cb) { m_onResize = std::move(cb); }

void* GlfwWindow::GetNativeHandle() const {
#if defined(_WIN32)
    // return glfwGetWin32Window(m_Window);
    return m_Window;
#elif defined(__APPLE__)
    return glfwGetCocoaWindow(m_Window);
#else
    return (void*)glfwGetX11Window(m_Window);
#endif
}

void GlfwWindow::updateDevicePixelRatio() {
    int fbW=0, fbH=0;
    glfwGetFramebufferSize(m_Window, &fbW, &fbH);

    // Protect against division by zero / first frame
    float dprX = (m_logicalW > 0) ? (float)fbW / (float)m_logicalW : 1.0f;
    float dprY = (m_logicalH > 0) ? (float)fbH / (float)m_logicalH : 1.0f;
    m_dpr = (dprX + dprY) * 0.5f;
}

void GlfwWindow::framebufferSizeCB(GLFWwindow* win, int w, int h) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    self->updateDevicePixelRatio();
    if (self->m_onResize) self->m_onResize(w, h, self->m_dpr);
}

void GlfwWindow::windowCloseCB(GLFWwindow* win) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    if (self->m_onClose) self->m_onClose();
    glfwSetWindowShouldClose(win, 1);
}

void GlfwWindow::contentScaleCB(GLFWwindow* win, float /*xscale*/, float /*yscale*/) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    self->updateDevicePixelRatio();
}