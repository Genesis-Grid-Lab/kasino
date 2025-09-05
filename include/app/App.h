#pragma once

#include "window/IWindow.h"
#include <memory>

class App{
public:
    explicit App(std::unique_ptr<IWindow> window);
    int Run();

private:
    std::unique_ptr<IWindow> m_Window;
};