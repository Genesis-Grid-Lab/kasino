#include "gfx/glad/GLRendererAPI.h"
#include "glad/glad.h"
#include "gfx/IVertexArray.h"

void GLRendererAPI::Init() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void GLRendererAPI::SetViewport(int x,int y,int w,int h){ glViewport(x,y,w,h); }
void GLRendererAPI::SetClearColor(float r,float g,float b,float a){ glClearColor(r,g,b,a); }
void GLRendererAPI::Clear(){ glClear(GL_COLOR_BUFFER_BIT); }
void GLRendererAPI::EnableBlend(bool enable){ if(enable) glEnable(GL_BLEND); else glDisable(GL_BLEND); }
void GLRendererAPI::DrawIndexed(const IVertexArray& /*vao*/, std::uint32_t count){
    glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, (const void*)0);
}
