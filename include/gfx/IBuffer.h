#pragma once

#include <cstddef>

enum class BufferType { Vertex, Index };

class IBuffer {
public:
  virtual ~IBuffer() = default;

  virtual BufferType Type() const = 0;
  virtual void SetData(const void *data, std::size_t bytes,
                       bool dynamic = false) = 0;
  virtual void Bind() const = 0;
  virtual void Unbind() const = 0;  
};
