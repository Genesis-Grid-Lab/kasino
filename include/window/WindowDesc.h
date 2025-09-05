#pragma once
#include <string>
#include "gfx/IGraphicsDevice.h"

struct WindowDesc{
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

  // Desired graphics API
  GraphicsAPI api = GraphicsAPI::None;

  // OpenGL hints (used if api == OpenGL && backend supports GL)
  int  glMajor = 3;
  int  glMinor = 3;
  bool glDebug = false;    
};
