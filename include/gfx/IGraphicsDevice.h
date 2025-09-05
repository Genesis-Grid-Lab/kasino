#pragma once

enum class GraphicsAPI{
    None,
    OpenGL,
    Vulkan
};

class IGraphicsDevice{
public:
    virtual ~IGraphicsDevice() = default;
    virtual GraphicsAPI API() const = 0;
    // TODO: CreateSwapchain, BeginFrame, EndFrame, etc.
};