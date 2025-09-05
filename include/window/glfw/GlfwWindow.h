#pragma once

#include "window/IWindow.h"
#include <string>

struct GLFWwindow;

class GlfwWindow : public IWindow{
public:
    explicit GlfwWindow(const WindowDesc& desc);
    ~GlfwWindow() override;

    bool ShouldClose() const override;
    void PollEvents() override;
    void SwapBuffers() override;

    Vector2 GetLogicalSize() const override;
    Vector2 GetWindowSize() const override;
    Vector2 GetFramebufferSize() const override;
    float GetDevicePixelRatio() const override;

    void SetCloseCallback(CloseCallback cb) override;
    void SetResizeCallback(ResizeCallback cb) override;

    void* GetNativeHandle() const override;

private:
    void updateDevicePixelRatio();
    static void framebufferSizeCB(GLFWwindow* win, int w, int h);
    static void windowCloseCB(GLFWwindow* win);
    static void contentScaleCB(GLFWwindow* win, float xscale, float yscale);

private:
    GLFWwindow* m_Window = nullptr;
    int m_logicalW = 360;
    int m_logicalH = 640;
    float m_dpr = 1.0f;

    CloseCallback m_onClose;
    ResizeCallback m_onResize;
};