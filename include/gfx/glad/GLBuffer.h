#pragma once
#include "gfx/IBuffer.h"

using GLuint = unsigned int;

class GLBuffer : public IBuffer{
public:
    GLBuffer(BufferType t);
    ~GLBuffer() override;

    BufferType Type() const override { return m_Type;}
    void SetData(const void* data, std::size_t bytes, bool dynamic=false) override;
    void UpdateSubData(std::size_t byteOffset, const void* data, std::size_t bytes) override;
    void Bind() const override;
    void Unbind() const override;

    GLuint id() const { return m_Id; }

private:
    GLuint m_Id = 0;
    BufferType m_Type;
};