#pragma once

#include "gfx/IShader.h"

using GLuint = unsigned int;
using GLenum = unsigned int;

class GLShader : public IShader {
public:
  GLShader(const std::string& filepath);
  ~GLShader() override {}

  void Destroy() override;

  bool CompileFromSource(const char *vs, const char *fs,
                         std::string *outLog = nullptr) override;
  void CompileFromSource(const std::unordered_map<GLenum, std::string>& shaderSources);
  void Bind() const override;
  void Unbind() const override;

  void SetFloat(const char *name, float v) override;
  void SetVec2(const char *name, const glm::vec2 &v) override;
  void SetMat4(const char *name, const glm::mat4 &v) override;

  void SetIntArray(const char* name, const int* v, int count) override;

private:
  std::string ReadFile(const std::string &filepath);
  std::unordered_map<GLenum, std::string> PreProcess(const std::string& source);  
  int uniformLoc(const char *name);

  GLuint m_Program = 0;
  std::unordered_map<std::string,int> m_Uniforms;  
};
