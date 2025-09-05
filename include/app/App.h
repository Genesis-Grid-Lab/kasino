#pragma once

#include "input/InputSystem.h"
#include "window/IWindow.h"
#include "gfx/IGraphicsDevice.h"

class App{
public:
  explicit App(Scope<IWindow> window, Scope<IGraphicsDevice> device);
  int Run();

private:
  Scope<IWindow> m_Window;
  Scope<IGraphicsDevice> m_Device;
  Scope<InputSystem> m_Input;
};
