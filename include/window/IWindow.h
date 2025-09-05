#pragma once

#include "core/Types.h"
#include "core/FactoryDesc.h"
#include "events/EventBus.h"
#include <utility>

class IWindow{
public:
  using CloseCallback = std::function<void()>;
  using ResizeCallback =
      std::function<void(int fbWidth, int fbHeight, float dpr)>;
  using GLProc = void* (*)(const char*);

  virtual ~IWindow() = default;

  virtual WindowAPI API() const = 0;

  virtual bool ShouldClose() const = 0;
  virtual void PollEvents() = 0;
  virtual void SwapBuffers() = 0;

  virtual std::pair<float, float> GetLogicalSize() const = 0;
  virtual std::pair<float, float> GetWindowSize() const = 0;
  virtual std::pair<float, float> GetFramebufferSize() const = 0;
  virtual float GetDevicePixelRatio() const = 0;

  virtual void SetCloseCallback(CloseCallback cb) = 0;
  virtual void SetResizeCallback(ResizeCallback cb) = 0;

  virtual EventBus &Events() = 0;

  virtual bool EnsureGLContext(int major, int minor, bool debug) {
    (void)major;
    (void)minor;
    (void)debug;
    return false;
  }
  virtual GLProc GetGLProcLoader() const { return nullptr; }
  virtual void SetSwapInterval(int interval) { (void)interval; }
  virtual bool IsVsyncEnabled() const { return false; }

  virtual float GetLogicalX(float windowX) const {
    auto [ww, wh] = GetWindowSize();
    auto [lw, lh] = GetLogicalSize();
    return ww ? (windowX / ww) * lw : windowX;
  }
  virtual float GetLogicalY(float windowY) const {
    auto [ww, wh] = GetWindowSize();
    auto [lw, lh] = GetLogicalSize();
    return wh ? (windowY / wh) * lh : windowY;
  }  

  virtual void* GetNativeHandle() const = 0;
};
