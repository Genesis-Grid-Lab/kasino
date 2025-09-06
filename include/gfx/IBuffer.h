#pragma once

#include <cstddef>

enum class BufferType { Vertex, Index };

class IBuffer {
public:
  virtual ~IBuffer() = default;

  virtual BufferType Type() const = 0;
  virtual void SetData(const void *data, std::size_t bytes,
                       bool dynamic = false) = 0;
  // Update a subrange; buffer must have been created with SetData first.
  virtual void UpdateSubData(std::size_t byteOffset, const void* data, std::size_t bytes) = 0;                       
  virtual void Bind() const = 0;
  virtual void Unbind() const = 0;  
};
