#include "core/Factory.h"
#include "gfx/glad/GLDevice.h"
#include "app/Game.h"
#include <memory>

class KasinoGame : public Game {
public:
    bool OnStart() override {
        // load assets here
        return true;
    }
    void OnRender() override {
      Render2D::DrawQuad({20, 20}, {120, 60}, {0.2f, 0.6f, 1.0f, 1.0f});
      Render2D::DrawQuad({200, 120}, {80, 120}, {0.9f, 0.2f, 0.3f, 1.0f});
    }
};

int main(){

  FactoryDesc desc;
  desc.title = "Kasino – Mobile Logical Scale";
  desc.logicalWidth  = 360;  // virtual “mobile” width
  desc.logicalHeight = 640;  // virtual “mobile” height
  desc.windowWidth   = 480;  // hint actual window size (can be 0 to auto)
  desc.windowHeight  = 800;
  desc.resizable     = false;
  desc.fullscreen    = false;
  desc.window_api    = WindowAPI::GLFW;
  desc.graphics_api  = GraphicsAPI::OpenGL;

  KasinoGame game;
  if (!game.Init(desc)) return 1;
  game.Run();
  game.Shutdown();

  return 0;
}
