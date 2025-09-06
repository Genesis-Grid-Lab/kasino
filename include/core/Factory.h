#pragma once

#include <memory>
#include "window/IWindow.h"
#include "core/FactoryDesc.h"
#include "gfx/IGraphicsDevice.h"
#include "gfx/IShader.h"
#include "gfx/IBuffer.h"
#include "gfx/IVertexArray.h"
#include "gfx/RendererAPI.h"
#include "gfx/ITexture2D.h"

class Factory {
public:
  static void SetGraphicsAPI(GraphicsAPI api);
  static GraphicsAPI GetGraphicsAPI() { return s_Gapi;}
  static void SetWindowAPI(WindowAPI api);
  static WindowAPI GetWindowAPI() { return s_Wapi;}

  static Scope<IWindow> CreateWindow(const FactoryDesc &desc);
  static Scope<IGraphicsDevice> CreateDevice(const FactoryDesc &desc);

  static Ref<IShader> CreateShader(const std::string& filepath);
  static Ref<IBuffer> CreateBuffer(BufferType type);
  static Ref<IVertexArray> CreateVertexArray();
  static std::shared_ptr<ITexture2D>   CreateTexture2D();

  static std::unique_ptr<RendererAPI>  CreateRendererAPI();

private:
  static GraphicsAPI s_Gapi;
  static WindowAPI s_Wapi;
};

