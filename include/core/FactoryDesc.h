#pragma once
#include <string>

enum class WindowAPI {
  None,
  GLFW,
  Native,
  Android,
  IOS
};

enum class GraphicsAPI{
  None,
  OpenGL,
  Vulkan
};


struct FactoryDesc{
  std::string title = "Kasino";
    
  //Logical (virtual) size
  int logicalWidth = 360;
  int logicalHeight = 640;

  // Inital windowed size (hint) - if 0, backend picks sensible defaults.
  int windowWidth = 0;
  int windowHeight = 0;

  bool resizable = true;
  bool fullscreen = false;

  bool vsync = true;    

  // Desired API
  WindowAPI window_api = WindowAPI::None;
  GraphicsAPI graphics_api = GraphicsAPI::None;

  // OpenGL hints (used if api == OpenGL && backend supports GL)
  int  glMajor = 3;
  int  glMinor = 3;
  bool glDebug = false;    
};
