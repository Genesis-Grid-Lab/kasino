#pragma once
#include "gfx/ITexture2D.h"
using GLuint = unsigned int;

class GLTexture2D : public ITexture2D {
public:
    GLTexture2D();
    ~GLTexture2D() override;

    bool LoadFromFile(const char* path, bool flipY=true) override;
    bool Create(uint32_t w, uint32_t h, int channels, const void* pixels) override;

    void Bind(uint32_t slot) const override;

    uint32_t Width() const override  { return m_w; }
    uint32_t Height() const override { return m_h; }

    GLuint id() const { return m_id; } // internal use if ever needed

private:
    void allocate(uint32_t w, uint32_t h, int channels);

private:
    GLuint m_id=0;
    uint32_t m_w=0, m_h=0;
    int m_channels=4;
};
