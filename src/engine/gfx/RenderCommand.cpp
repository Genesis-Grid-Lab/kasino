#include "gfx/RenderCommand.h"
#include "gfx/Render2D.h"

std::unique_ptr<RendererAPI> RenderCommand::s_API;

void RenderCommand::Init(std::unique_ptr<RendererAPI> api) {
    s_API = std::move(api);
    if (s_API) s_API->Init();

    Render2D::Initialize();
}

void RenderCommand::Shutdown(){ Render2D::Shutdown(); s_API.reset();}
void RenderCommand::SetViewport(int x,int y,int w,int h) { s_API->SetViewport(x,y,w,h); }
void RenderCommand::SetClearColor(float r,float g,float b,float a){ s_API->SetClearColor(r,g,b,a); }
void RenderCommand::Clear(){ s_API->Clear(); }
void RenderCommand::EnableBlend(bool e){ s_API->EnableBlend(e); }
void RenderCommand::DrawIndexed(const IVertexArray& vao, std::uint32_t n){ s_API->DrawIndexed(vao, n); }
