#pragma once

#include <memory>
#include "window/IWindow.h"
#include "window/WindowDesc.h"

std::unique_ptr<IWindow> CreateWindow(const WindowDesc& desc);