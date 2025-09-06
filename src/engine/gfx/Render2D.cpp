#include "gfx/Render2D.h"
#include "core/Factory.h"
#include "gfx/RenderCommand.h"
#include <glm/gtc/type_ptr.hpp>
#include <cstring>

static const char* VS = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;
layout(location=2) in vec2 aUV;
layout(location=3) in float aTexIndex;
layout(location=4) in float aTiling;
uniform mat4 uViewProj;
out vec4 vColor; out vec2 vUV; out float vTexIndex; out float vTiling;
void main(){ vColor=aColor; vUV=aUV; vTexIndex=aTexIndex; vTiling=aTiling;
             gl_Position = uViewProj * vec4(aPos,0,1); } )";

static const char* FS = R"(#version 330 core
in vec4 vColor; in vec2 vUV; in float vTexIndex; in float vTiling;
out vec4 FragColor; uniform sampler2D uTextures[16];
vec4 sampleT(float idx, vec2 uv){ int i=int(idx+0.5); return texture(uTextures[i], uv * vTiling); }
void main(){ FragColor = vColor * sampleT(vTexIndex, vUV); } )";

struct QuadVertex { float x,y; float r,g,b,a; float u,v; float texIndex; float tiling; };

static bool s_ready = false;
static std::shared_ptr<IVertexArray> m_vao;
static std::shared_ptr<IBuffer>      m_vbo;
static std::shared_ptr<IBuffer>      m_ibo;
static std::shared_ptr<IShader>      m_shader;
static std::shared_ptr<ITexture2D>   m_white;
static std::vector<QuadVertex> m_cpu;

bool Render2D::Initialize(){
    if(s_ready) return true;

    m_vao = Factory::CreateVertexArray();
    m_vbo = Factory::CreateBuffer(BufferType::Vertex);
    m_ibo = Factory::CreateBuffer(BufferType::Index);
    m_shader = Factory::CreateShader();

    std::string log;
    if(!m_shader->CompileFromSource(VS,FS,&log)){ fprintf(stderr,"[Render2D] Shader: %s\n", log.c_str()); return false; }

    int samplers[16]; for(int i=0;i<16;i++) samplers[i]=i;
    m_shader->Bind();
    m_shader->SetIntArray("uTextures", samplers, 16);

    m_cpu.resize(MaxVerts);
    m_vbo->SetData(nullptr, sizeof(QuadVertex)*MaxVerts, true);

    std::vector<uint32_t> idx(MaxIndices);
    uint32_t off=0;
    for(uint32_t i=0;i<MaxIndices;i+=6){
        idx[i+0]=off+0; idx[i+1]=off+1; idx[i+2]=off+2;
        idx[i+3]=off+0; idx[i+4]=off+2; idx[i+5]=off+3;
        off+=4;
    }
    m_ibo->SetData(idx.data(), idx.size()*sizeof(uint32_t), false);

    m_vao->Bind(); m_vbo->Bind();
    m_vao->EnableAttrib(0,2,0x1406,false,sizeof(QuadVertex),offsetof(QuadVertex,x));
    m_vao->EnableAttrib(1,4,0x1406,false,sizeof(QuadVertex),offsetof(QuadVertex,r));
    m_vao->EnableAttrib(2,2,0x1406,false,sizeof(QuadVertex),offsetof(QuadVertex,u));
    m_vao->EnableAttrib(3,1,0x1406,false,sizeof(QuadVertex),offsetof(QuadVertex,texIndex));
    m_vao->EnableAttrib(4,1,0x1406,false,sizeof(QuadVertex),offsetof(QuadVertex,tiling));
    m_vao->Unbind();

    m_white = Factory::CreateTexture2D();
    const std::uint32_t white = 0xFFFFFFFFu;
    m_white->Create(1,1,4,&white);

    s_ready = true;
    ResetStats();
    startBatch();
    return true;
}

void Render2D::BeginScene(const glm::mat4& viewProj, int fbW, int fbH){
    (void)fbW; (void)fbH; // optional: RenderCommand::SetViewport(0,0,fbW,fbH);
    m_viewProj = viewProj;
    startBatch();
}

void Render2D::EndScene(){ flush(); }

void Render2D::Clear(float r,float g,float b,float a){
    RenderCommand::SetClearColor(r,g,b,a);
    RenderCommand::Clear();
}

void Render2D::startBatch(){
    m_quadCount=0; m_texCount=0;
    std::memset(m_texSlots,0,sizeof(m_texSlots));
    m_texSlots[m_texCount++] = m_white.get();
}

void Render2D::flush(){
    if(m_quadCount==0) return;

    const uint32_t vcount = m_quadCount*4;
    m_vbo->UpdateSubData(0, m_cpu.data(), sizeof(QuadVertex)*vcount);

    for(uint32_t i=0;i<m_texCount;i++) m_texSlots[i]->Bind(i);

    m_shader->Bind();
    m_shader->SetMat4("uViewProj", m_viewProj);

    RenderCommand::DrawIndexed(*m_vao, m_quadCount * 6);

    m_stats.drawCalls++;
    startBatch();
}

int Render2D::getTextureSlot(ITexture2D* t){
    for(uint32_t i=0;i<m_texCount;i++) if(m_texSlots[i]==t) return (int)i;
    if(m_texCount>=MaxTexSlots) flush();
    m_texSlots[m_texCount]=t; return (int)m_texCount++;
}

void Render2D::submitQuad(float x,float y,float w,float h,const float c[4],int slot,
                          float u0,float v0,float u1,float v1,float tiling){
    if(m_quadCount>=MaxQuads) flush();
    const float x0=x,y0=y,x1=x+w,y1=y+h;
    auto* v = &m_cpu[m_quadCount*4];
    v[0] = {x0,y0, c[0],c[1],c[2],c[3], u0,v0,(float)slot,tiling};
    v[1] = {x0,y1, c[0],c[1],c[2],c[3], u0,v1,(float)slot,tiling};
    v[2] = {x1,y1, c[0],c[1],c[2],c[3], u1,v1,(float)slot,tiling};
    v[3] = {x1,y0, c[0],c[1],c[2],c[3], u1,v0,(float)slot,tiling};
    m_quadCount++; m_stats.quadCount++;
}

void Render2D::DrawQuad(float x,float y,float w,float h,float r,float g,float b,float a){
    const float c[4]={r,g,b,a}; submitQuad(x,y,w,h,c, 0, 0,0,1,1, 1.0f);
}
void Render2D::DrawQuad(float x,float y,float w,float h,ITexture2D* tex,
                        float tr,float tg,float tb,float ta,
                        float u0,float v0,float u1,float v1,float tiling){
    const float c[4]={tr,tg,tb,ta}; int slot=getTextureSlot(tex);
    submitQuad(x,y,w,h,c, slot, u0,v0,u1,v1, tiling);
}
