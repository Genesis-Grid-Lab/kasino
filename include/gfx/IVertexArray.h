#pragma once
#include <cstddef>

class IVertexArray {
public:
    virtual ~IVertexArray() = default;

    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;

    // index: shader location; comps: 1..4; type: GL_FLOAT (pass-through as unsigned int)
    virtual void EnableAttrib(unsigned int index, int comps, unsigned int type,
                              bool normalized, int stride, std::size_t offset) = 0;
};