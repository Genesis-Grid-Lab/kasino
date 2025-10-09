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
#include "audio/IAudioDevice.h"

class Factory {
public:
  static void SetDesc(const FactoryDesc &desc);
  static void SetGraphicsAPI(GraphicsAPI api);
  static GraphicsAPI GetGraphicsAPI() { return s_Desc.graphics_api;}
  static void SetWindowAPI(WindowAPI api);
  static WindowAPI GetWindowAPI() { return s_Desc.window_api;}

  static Scope<IWindow> CreateWindow();
  static Scope<IGraphicsDevice> CreateGraphicsDevice();
  static Scope<IAudioDevice> CreateAudioDevice();

  static Ref<IShader> CreateShader(const std::string& filepath);
  static Ref<IBuffer> CreateBuffer(BufferType type);
  static Ref<IVertexArray> CreateVertexArray();
  static std::shared_ptr<ITexture2D>   CreateTexture2D();

  static std::unique_ptr<RendererAPI>  CreateRendererAPI();

private:
  static FactoryDesc s_Desc;
};

