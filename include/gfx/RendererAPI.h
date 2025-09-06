#pragma once

#include <cstdint>

#include "core/FactoryDesc.h"
class IVertexArray;

class RendererAPI{
public:
    virtual ~RendererAPI() = default;

    virtual void Init() = 0;

    virtual void SetViewport(int x, int y, int w, int h) = 0;

    virtual void SetClearColor(float r, float g, float b, float a) = 0;
    virtual void Clear() = 0;

    virtual void EnableBlend(bool enable) = 0;

    // Draw indexed using currently bound VAO & index buffer
    virtual void DrawIndexed(const IVertexArray& vao, std::uint32_t indexCount) = 0;
};