#include "window/WindowFactory.h"
#include "app/App.h"
#include <memory>

int main(){

    WindowDesc desc;
    desc.title = "Kasino – Mobile Logical Scale";
    desc.logicalWidth  = 360;  // virtual “mobile” width
    desc.logicalHeight = 640;  // virtual “mobile” height
    desc.windowWidth   = 480;  // hint actual window size (can be 0 to auto)
    desc.windowHeight  = 800;
    desc.resizable     = true;
    desc.fullscreen    = false;
    desc.api           = GraphicsAPI::None; // later: OpenGL / Vulkan / Urban

    auto window = CreateWindow(desc);
    App app(std::move(window));
    return app.Run();
}