#pragma once

#include <memory>
#include "window/IWindow.h"
#include "core/FactoryDesc.h"
#include "gfx/IGraphicsDevice.h"
#include "gfx/IShader.h"
#include "gfx/IBuffer.h"
#include "gfx/IVertexArray.h"

class Factory {
public:
  static void SetGraphicsAPI(GraphicsAPI api);
  static GraphicsAPI GetGraphicsAPI() { return s_Gapi;}
  static void SetWindowAPI(WindowAPI api);
  static WindowAPI GetWindowAPI() { return s_Wapi;}

  static Scope<IWindow> CreateWindow(const FactoryDesc &desc);
  static Scope<IGraphicsDevice> CreateDevice(const FactoryDesc &desc);

  static Ref<IShader> CreateShader();
  static Ref<IBuffer> CreateBuffer(BufferType type);
  static Ref<IVertexArray> CreateVertexArray();

private:
  static GraphicsAPI s_Gapi;
  static WindowAPI s_Wapi;
};

