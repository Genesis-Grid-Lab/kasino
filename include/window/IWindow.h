#pragma once

#include "core/Types.h"
#include "window/WindowDesc.h"
#include "events/EventBus.h"

class IWindow{
public:
    using CloseCallback = std::function<void()>;
    using ResizeCallback = std::function<void(int fbWidth, int fbHeight, float dpr)>;

    virtual ~IWindow() = default;

    virtual bool ShouldClose() const = 0;
    virtual void PollEvents() = 0;
    virtual void SwapBuffers() = 0;

    virtual Vector2 GetLogicalSize() const = 0;
    virtual Vector2 GetWindowSize() const = 0;
    virtual Vector2 GetFramebufferSize() const = 0;
    virtual float GetDevicePixelRatio() const = 0;

    virtual void SetCloseCallback(CloseCallback cb) = 0;
    virtual void SetResizeCallback(ResizeCallback cb) = 0;

    virtual EventBus &Events() = 0;    

    virtual void* GetNativeHandle() const = 0;
};
