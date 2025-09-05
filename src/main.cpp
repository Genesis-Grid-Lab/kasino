#include "core/Factory.h"
#include "gfx/glad/GLDevice.h"
#include "app/App.h"
#include <memory>

int main(){

    FactoryDesc desc;
    desc.title = "Kasino – Mobile Logical Scale";
    desc.logicalWidth  = 360;  // virtual “mobile” width
    desc.logicalHeight = 640;  // virtual “mobile” height
    desc.windowWidth   = 480;  // hint actual window size (can be 0 to auto)
    desc.windowHeight  = 800;
    desc.resizable     = true;
    desc.fullscreen    = false;
    desc.window_api    = WindowAPI::GLFW;
    desc.graphics_api  = GraphicsAPI::OpenGL;

    auto window = Factory::CreateWindow(desc);
    auto device = Factory::CreateDevice(desc);
    App app(std::move(window), std::move(device));
    return app.Run();
}
