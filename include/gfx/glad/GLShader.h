#pragma once

#include "gfx/IShader.h"

using GLuint = unsigned int;

class GLShader : public IShader {
public:
  GLShader() = default;
  ~GLShader() override;

  bool CompileFromSource(const char *vs, const char *fs,
                         std::string *outLog = nullptr) override;
  void Bind() const override;
  void Unbind() const override;

  void SetFloat(const char *name, float v) override;
  void SetVec2(const char *name, const glm::vec2 &v) override;
  void SetMat4(const char *name, const glm::mat4 &v) override;

private:
  int uniformLoc(const char *name);

  GLuint m_Program = 0;
  std::unordered_map<std::string,int> m_Uniforms;  
};
