#include "core/Factory.h"
#include "Kasino/KasinoGame.h"

int main() {
  FactoryDesc desc;
  desc.title = "Kasino – Mobile Logical Scale";
  desc.logicalWidth = 480;
  desc.logicalHeight = 800;
  desc.windowWidth = 480;
  desc.windowHeight = 800;
  desc.resizable = false;
  desc.fullscreen = false;
  desc.window_api = WindowAPI::GLFW;
  desc.graphics_api = GraphicsAPI::OpenGL;

  KasinoGame game;
  if (!game.Init(desc)) return 1;
  game.Run();
  game.Shutdown();
  return 0;
}

