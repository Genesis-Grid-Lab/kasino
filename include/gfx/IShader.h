#pragma once

#include "core/Types.h"

class IShader{
public:
  virtual ~IShader() = default;

  // Compile and link from two source strings
  virtual bool CompileFromSource(const char *vs, const char *fs,
                                 std::string *outLog = nullptr) = 0;

  virtual void Bind() const = 0;
  virtual void Unbind() const = 0;

  // uniform helpers
  virtual void SetFloat(const char *name, float v) = 0;
  virtual void SetVec2(const char *name, const glm::vec2& v) = 0;
  virtual void SetMat4(const char* name, const glm::mat4& v) = 0;  
  virtual void SetIntArray(const char* name, const int* v, int count) = 0;
};
