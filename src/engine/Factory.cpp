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

Scope<IWindow> Factory::CreateWindow(const WindowDesc &desc) {
  switch(s_Wapi){
  case WindowAPI::GLFW:
    return CreateScope<GlfwWindow>(desc);
  }

  // static_assert(false, "No window backend selected");
  return nullptr;
}

Scope<IGraphicsDevice> Factory::CreateDevice() {
  switch(s_Gapi){
  case GraphicsAPI::OpenGL:
    return CreateScope<GLDevice>();
  }

  return nullptr;
}
