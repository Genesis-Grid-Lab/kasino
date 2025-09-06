#pragma once
#include <memory>
#include "gfx/RendererAPI.h"

class RenderCommand {
public:
    static void Init(std::unique_ptr<RendererAPI> api);

    static void SetViewport(int x,int y,int w,int h);
    static void SetClearColor(float r,float g,float b,float a);
    static void Clear();

    static void EnableBlend(bool e);

    static void DrawIndexed(const IVertexArray& vao, std::uint32_t indexCount);

private:
    static std::unique_ptr<RendererAPI> s_API;
};