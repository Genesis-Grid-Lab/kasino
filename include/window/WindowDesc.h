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

    // Desired graphics API
    GraphicsAPI api = GraphicsAPI::None;
};