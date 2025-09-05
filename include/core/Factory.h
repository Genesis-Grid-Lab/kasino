#pragma once

#include <memory>
#include "window/IWindow.h"
#include "window/WindowDesc.h"
#include "gfx/IGraphicsDevice.h"

class Factory {
public:
  static GraphicsAPI GetGraphicsAPI() { return s_Gapi;}
  static WindowAPI GetWindowAPI() { return s_Wapi;}
  static Scope<IWindow> CreateWindow(const WindowDesc &desc);
  static Scope<IGraphicsDevice> CreateDevice();

private:
  static GraphicsAPI s_Gapi;
  static WindowAPI s_Wapi;
};

