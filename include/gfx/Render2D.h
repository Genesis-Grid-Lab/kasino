#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "gfx/ITexture2D.h"
#include "gfx/IShader.h"
#include "gfx/IBuffer.h"
#include "gfx/IVertexArray.h"

class Render2D {
public:
    static bool Initialize();

    // Provide camera VP and framebuffer size (fb used for viewport if you want).
    static void BeginScene(const glm::mat4& viewProj, int fbWidth, int fbHeight);
    static void EndScene();

    static void Clear(float r,float g,float b,float a);

    static void DrawQuad(float x,float y,float w,float h, float r,float g,float b,float a);
    static void DrawQuad(float x,float y,float w,float h, ITexture2D* tex,
                  float tr=1,float tg=1,float tb=1,float ta=1,
                  float u0=0,float v0=0,float u1=1,float v1=1,float tiling=1);

    struct Stats { uint32_t drawCalls=0, quadCount=0; };
    Stats GetStats() const { return m_stats; }
    static void  ResetStats() { m_stats = {}; }

private:    
    static constexpr uint32_t MaxQuads=10000, MaxVerts=MaxQuads*4, MaxIndices=MaxQuads*6, MaxTexSlots=16;

    static void startBatch();
    static void flush();
    int  getTextureSlot(ITexture2D* tex);
    void submitQuad(float x,float y,float w,float h,const float color[4],int slot,
                    float u0,float v0,float u1,float v1,float tiling);

private:                        
    uint32_t m_quadCount=0;

    ITexture2D* m_texSlots[MaxTexSlots]{};
    uint32_t    m_texCount=0;

    glm::mat4   m_viewProj {1.0f};

    Stats m_stats;
    bool  m_ready=false;
};
