#pragma once
#include "core/Types.h"

#include "gfx/Camera2D.h"
#include "gfx/Render2D.h"
#include "gfx/IGraphicsDevice.h"
#include "gfx/RenderCommand.h"
#include "core/Factory.h"
#include "window/IWindow.h"

// forward-declare to avoid depending on its exact fields
struct FactoryDesc;

class Game {
 public:
  Game() = default;
  virtual ~Game() = default;

  // NOTE: now takes FactoryDesc instead of WindowDesc
  bool Init(const FactoryDesc& desc);
  void Run();
  void Shutdown();
  void Stop() { m_Running = false; }

 protected:
  virtual bool OnStart() { return true; }
  virtual void OnUpdate(float dtSeconds) {}
  virtual void OnRender() {}
  virtual void OnResize(int fbWidth, int fbHeight) {}
  virtual void OnStop() {}

 protected:
  Scope<IWindow>         m_Window;
  Scope<IGraphicsDevice> m_Device;
  Camera2D             m_Camera;

  int m_fbWidth  = 0;
  int m_fbHeight = 0;

 private:
  bool m_Running = false;
};
