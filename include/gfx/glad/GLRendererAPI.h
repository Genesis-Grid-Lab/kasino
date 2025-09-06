#pragma once
#include "gfx/RendererAPI.h"

class GLRendererAPI : public RendererAPI {
public:
    void Init() override;
    void SetViewport(int x,int y,int w,int h) override;
    void SetClearColor(float r,float g,float b,float a) override;
    void Clear() override;
    void EnableBlend(bool enable) override;
    void DrawIndexed(const class IVertexArray& vao, std::uint32_t indexCount) override;
};
