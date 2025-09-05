#pragma once
#include "window/IWindow.h"

class AndroidWindow : public IWindow {
public:
    explicit AndroidWindow(const WindowDesc& desc);
    ~AndroidWindow() override;

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

    EventBus& Events() override { return m_events; }

private:
    int m_logicalW, m_logicalH;
    int m_winW, m_winH;  // you’ll set from AConfiguration / ANativeWindow
    float m_dpr;
    bool m_shouldClose=false;
    CloseCallback m_onClose;
    ResizeCallback m_onResize;
    EventBus m_events;
};
