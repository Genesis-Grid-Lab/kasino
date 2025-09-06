#pragma once
#include <cstdint>

class ITexture2D {
public:
    virtual ~ITexture2D() = default;

    // Load from disk (RGBA8 preferred). flipY = true for typical 2D coords.
    virtual bool LoadFromFile(const char* path, bool flipY = true) = 0;

    // Create from memory (RGBA8 or RGB8; channels = 3 or 4).
    virtual bool Create(uint32_t width, uint32_t height, int channels, const void* pixels) = 0;

    virtual void Bind(uint32_t slot) const = 0;

    virtual uint32_t Width()  const = 0;
    virtual uint32_t Height() const = 0;
};
