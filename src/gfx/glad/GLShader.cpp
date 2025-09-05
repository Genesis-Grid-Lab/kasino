#include "gfx/glad/GLShader.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

static bool compile(GLuint sh, const char* src, std::string* log){
  glShaderSource(sh, 1, &src, nullptr);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

  if (!ok && log) {
    GLint len = 0;
    glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> buf(len + 1);
    glGetShaderInfoLog(sh, len, nullptr, buf.data());
    *log += buf.data();
  }

  return ok==GL_TRUE;
}

GLShader::~GLShader() {
  if (m_Program)
    glDeleteProgram(m_Program);
}

bool GLShader::CompileFromSource(const char *vs, const char *fs,
                                 std::string *outLog) {
  GLuint v = glCreateShader(GL_VERTEX_SHADER),
         f = glCreateShader(GL_FRAGMENT_SHADER);
  std::string log;

  if (!compile(v, vs, &log) || !compile(f, fs, &log)) {
    if (outLog)
      *outLog = log;
    glDeleteShader(v);
    glDeleteShader(f);
    return false;    
  }

  GLuint p = glCreateProgram();
  glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
  glDeleteShader(v); glDeleteShader(f);
  GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
  if(!ok){ GLint len=0; glGetProgramiv(p,GL_INFO_LOG_LENGTH,&len);
    std::vector<char> buf(len + 1);
    glGetProgramInfoLog(p, len, nullptr, buf.data());    
    if (outLog)
      *outLog = buf.data();
    glDeleteProgram(p);
    return false;    
  }
  m_Program = p;
  m_Uniforms.clear();
  return true;  
}

void GLShader::Bind() const { glUseProgram(m_Program); }
void GLShader::Unbind() const { glUseProgram(0); }

int GLShader::uniformLoc(const char* name){
    auto it=m_Uniforms.find(name);
    if(it!=m_Uniforms.end()) return it->second;
    int loc=glGetUniformLocation(m_Program,name);
    m_Uniforms[name]=loc; return loc;
}
void GLShader::SetFloat(const char* name,float v){ glUniform1f(uniformLoc(name),v); }
void GLShader::SetVec2 (const char* name, const glm::vec2& v){ glUniform2f(uniformLoc(name),v.x,v.y); }
void GLShader::SetMat4 (const char* name,const glm::mat4& v){ glUniformMatrix4fv(uniformLoc(name),1,GL_FALSE,glm::value_ptr(v)); }
