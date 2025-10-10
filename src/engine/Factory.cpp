#include "core/Factory.h"
#include "gfx/IGraphicsDevice.h"

#if defined(WINDOW_BACKEND_GLFW)
  #include "window/glfw/GlfwWindow.h"
#elif defined(WINDOW_BACKEND_NATIVE)
  #include "window/native/NativeWindow.h"
#elif defined(WINDOW_BACKEND_ANDROID)
  #include "window/android/AndroidWindow.h"
#elif defined(WINDOW_BACKEND_IOS)
  #include "window/ios/IosWindow.h"
#endif

#include "gfx/glad/GLDevice.h"
#include "gfx/glad/GLBuffer.h"
#include "gfx/glad/GLShader.h"
#include "gfx/glad/GLVertexArray.h"
#include "gfx/glad/GLTexture2D.h"
#include "gfx/glad/GLRendererAPI.h"

#include "audio/null/NullAudioDevice.h"
#include "audio/miniaudio/MiniaudioDevice.h"

FactoryDesc Factory::s_Desc = FactoryDesc{};

void Factory::SetDesc(const FactoryDesc& desc){
  s_Desc = desc;
}

void Factory::SetGraphicsAPI(GraphicsAPI api){ s_Desc.graphics_api = api;}
void Factory::SetWindowAPI(WindowAPI api){ s_Desc.window_api = api;}

Scope<IWindow> Factory::CreateWindow() {
  switch(s_Desc.window_api){
  case WindowAPI::GLFW:
    return CreateScope<GlfwWindow>(s_Desc);
  }

  // static_assert(false, "No window backend selected");
  return nullptr;
}

Scope<IGraphicsDevice> Factory::CreateGraphicsDevice() {
  switch(s_Desc.graphics_api){
  case GraphicsAPI::OpenGL:
    return CreateScope<GLDevice>(s_Desc);
  }

  return nullptr;
}

Scope<IAudioDevice> Factory::CreateAudioDevice(){
  switch(s_Desc.audio_api){
    case AudioAPI::Miniaudio: return CreateScope<MiniaudioDevice>();
    case AudioAPI::Null:
    default: return CreateScope<NullAudioDevice>();
  }
}


Ref<IShader> Factory::CreateShader(const std::string& filepath){
  switch(s_Desc.graphics_api){
  case GraphicsAPI::OpenGL:
    return CreateRef<GLShader>(filepath);
  }

  return nullptr;
}
Ref<IBuffer> Factory::CreateBuffer(BufferType type){
  switch(s_Desc.graphics_api){
  case GraphicsAPI::OpenGL:
    return CreateRef<GLBuffer>(type);
  }

  return nullptr;
}
Ref<IVertexArray> Factory::CreateVertexArray(){
  switch(s_Desc.graphics_api){
  case GraphicsAPI::OpenGL:
    return CreateRef<GLVertexArray>();
  }

  return nullptr;
}

std::shared_ptr<ITexture2D>   Factory::CreateTexture2D()   { 
  switch(s_Desc.graphics_api){ case GraphicsAPI::OpenGL: return std::make_shared<GLTexture2D>(); default: return nullptr; } 
}
std::unique_ptr<RendererAPI>  Factory::CreateRendererAPI() { 
  switch(s_Desc.graphics_api){ case GraphicsAPI::OpenGL: return std::make_unique<GLRendererAPI>(); default: return nullptr; } 
}
