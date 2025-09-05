#pragma once

#include <memory>
#include "window/IWindow.h"
#include "window/WindowDesc.h"

Scope<IWindow> CreateWindow(const WindowDesc& desc);
