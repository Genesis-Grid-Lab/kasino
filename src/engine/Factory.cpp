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

// Scope<IWindow> CreateWindow(const WindowDesc& desc){
// #if defined(WINDOW_BACKEND_GLFW)
//     return CreateScope<GlfwWindow>(desc);
// #elif defined(WINDOW_BACKEND_NATIVE)
//     return CreateScope<NativeWindow>(desc);
// #elif defined(WINDOW_BACKEND_ANDROID)
//     return CreateScope<AndroidWindow>(desc);
// #elif defined(WINDOW_BACKEND_IOS)
//     return CreateScope<IosWindow>(desc);
// #else
//     static_assert(false, "No window backend selected");
//     return {};
// #endif
// }

// Scope<IGraphicsDevice> CreateDevice() { return CreateScope<GLDevice>(); }

GraphicsAPI Factory::s_Gapi = GraphicsAPI::OpenGL;
WindowAPI Factory::s_Wapi = WindowAPI::GLFW;

void Factory::SetGraphicsAPI(GraphicsAPI api){ s_Gapi = api;}
void Factory::SetWindowAPI(WindowAPI api){ s_Wapi = api;}

Scope<IWindow> Factory::CreateWindow(const FactoryDesc &desc) {
  switch(s_Wapi){
  case WindowAPI::GLFW:
    return CreateScope<GlfwWindow>(desc);
  }

  // static_assert(false, "No window backend selected");
  return nullptr;
}

Scope<IGraphicsDevice> Factory::CreateDevice(const FactoryDesc &desc) {
  switch(s_Gapi){
  case GraphicsAPI::OpenGL:
    return CreateScope<GLDevice>(desc);
  }

  return nullptr;
}


Ref<IShader> Factory::CreateShader(const std::string& filepath){
  switch(s_Gapi){
  case GraphicsAPI::OpenGL:
    return CreateRef<GLShader>(filepath);
  }

  return nullptr;
}
Ref<IBuffer> Factory::CreateBuffer(BufferType type){
  switch(s_Gapi){
  case GraphicsAPI::OpenGL:
    return CreateRef<GLBuffer>(type);
  }

  return nullptr;
}
Ref<IVertexArray> Factory::CreateVertexArray(){
  switch(s_Gapi){
  case GraphicsAPI::OpenGL:
    return CreateRef<GLVertexArray>();
  }

  return nullptr;
}

std::shared_ptr<ITexture2D>   Factory::CreateTexture2D()   { 
  switch(s_Gapi){ case GraphicsAPI::OpenGL: return std::make_shared<GLTexture2D>(); default: return nullptr; } 
}
std::unique_ptr<RendererAPI>  Factory::CreateRendererAPI() { 
  switch(s_Gapi){ case GraphicsAPI::OpenGL: return std::make_unique<GLRendererAPI>(); default: return nullptr; } 
}
